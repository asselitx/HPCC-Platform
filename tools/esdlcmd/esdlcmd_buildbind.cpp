/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2020 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#include "jliball.hpp"
#include "esdlcmd_core.hpp"
#include "build-config.h"
#include "esdlcmdutils.hpp"
//#include <xpp/XmlPullParser.h>
#include "fxpp/FragmentedXmlPullParser.hpp"
#include "fxpp/FragmentedXmlAssistant.hpp"
#include <memory>
#include <unordered_set>
#include <vector>

using namespace xpp;
using namespace fxpp;
using namespace fxpp::fxa;

//#define _MANIFEST_DEBUG_

#define ESDL_BUILD_BINDING_ERR  23

#define MANIFEST_URI                "urn:hpcc:esdl:manifest"

#define MANIFEST_TAG_TRANSFORMS     "Transforms"
#define MANIFEST_TAG_TRANSFORM      "Transform"
#define MANIFEST_TAG_SCRIPTS        "Scripts"
#define MANIFEST_TAG_MANIFEST       "Manifest"
#define MANIFEST_TAG_BINDING        "ServiceBinding"
#define MANIFEST_TAG_MANIFEST       "Manifest"
#define MANIFEST_TAG_INTERFACE      "ServiceInterface"
#define MANIFEST_TAG_INCLUDE        "Include"
#define MANIFEST_TAG_LOGGING        "Logging"

#define BUNDLE_START                "<EsdlBundle>\n"
#define BUNDLE_END                  "</EsdlBundle>"

class EsdlBuildBindingCmd : public EsdlCmdCommon
{
    protected:
        // Command-line options
        StringAttr optManifestPath;
        StringAttr optService;
        StringBuffer optIncludePath;
        StringAttr optOutputPath;
        std::vector<std::string> esdlIncludes;
        std::vector<std::string> scriptIncludes;

        // Input
        StringBuffer manifest;

        // Holds serialized output
        StringBuffer binding;
        //StringBuffer bundle;

        EsdlCmdHelper cmdHelper;
        bool inScripts = false;
        bool isBundle = false;

    public:

        EsdlBuildBindingCmd() {}

        int processCMD() override
        {   bool result = false;

            try
            {
                printf("Manifest: %s\n", optManifestPath.str());

                #ifdef _MANIFEST_DEBUG_
                if (result)
                {
                    StringBuffer tmp;
                    toXML(manifest, tmp);
                    DBGLOG("Manifest contents: (%s)", tmp.str());
                }
                #endif

                result = processManifest();

                if (!result)
                {
                    UERRLOG("Error: Failed processsing binding template file. Exiting.");
                    return result;
                }

                result = outputBinding();
                if (!result)
                {
                    UERRLOG("Error: Failed writing binding output file. Exiting.");
                    return result;
                }

                UERRLOG("Success");

            } catch (IException* e) {
                StringBuffer msg;
                e->errorMessage(msg);
                UERRLOG("Error running build-binding: %s. \nExiting.\n", msg.str());
                e->Release();
                result = false;
            } catch (XmlPullParserException& xppex) {
                UERRLOG("Error running build-binding: %s.\nExiting.\n", xppex.what());
                result = false;
            } catch (...) {
                UERRLOG("Unknown error running build-binding. Exiting\n");
                result = false;
            }

            #ifdef _MANIFEST_DEBUG_
            DBGLOG("Binding Output:\n%s", binding.str());
            #endif

            return result;
        }

        bool parseCommandLineOptions(ArgvIterator &iter) override
        {
            bool result = false;

            if (iter.done())
            {
                usage();
            } else {

                //First parameters' order is fixed.
                const char *arg = iter.query();
                if (*arg != '-')
                {
                    optManifestPath.set(arg);
                }
                else
                {
                    fprintf(stderr, "\noption detected before required parameters: %s\n", arg);
                    usage();
                    return false;
                }

                for (; !iter.done(); iter.next())
                {
                    //if (iter.matchFlag(optNoCdata, ESDLOPT_MANIFEST_NOCDATA))
                    //    result = true;
                    StringAttr oneOption;
                    if (iter.matchOption(oneOption, ESDLOPT_INCLUDE_PATH) || iter.matchOption(oneOption, ESDLOPT_INCLUDE_PATH_S))
                    {
                        esdlIncludes.push_back(oneOption.get());
                        result = true;
                    }
                    if (iter.matchOption(oneOption, ESDLOPT_SCRIPT_INCLUDE_PATH) || iter.matchOption(oneOption, ESDLOPT_SCRIPT_INCLUDE_PATH_S))
                    {
                        scriptIncludes.push_back(oneOption.get());
                        result = true;
                    }
                    if (iter.matchOption(optService, ESDLOPT_SERVICE))
                        result = true;
                    if (iter.matchOption(optOutputPath, ESDLOPT_MANIFEST_OUTFILE))
                        result = true;
                }
            }
            return result;
        }

        void usage() override
        {
            //   >-------------------------- 80 columns ------------------------------------------<
            puts("Usage:\n");
            puts("esdl build-binding <manifest-file> [options]\n");
            puts("Options:");
            puts("    -i | --script-include-path <path>");
            puts("                        Search path for ESDL Scripts included in the manifest. Use");
            puts("                        once for each path.");
            puts("    -I | --include-path <path>");
            puts("                        Search path for ESDL Definition interface files included in");
            puts("                        the manifest. Use once for each path.");
            puts("    --outfile <filename>");
            puts("                        Path and name of the output file");
            puts("    --service <name>    Name of the service to include for the ESDL Definition. If");
            puts("                        provided, overrides the ServiceDefinition[@service]");
            puts("                        value in the manifest");

            EsdlCmdCommon::usage();
        }

        bool finalizeOptions(IProperties* globals) override
        {
            return true;
        }

    private:


        class IncludeLoader : public CInterfaceOf<fxpp::fxa::IExternalContentLoader>
        {
            public:
                virtual bool loadAndInject(const IExternalDetection& detected, IExternalContentInjector& injector) const override
                {
                    std::string path(detected.queryContentLocation());
                    std::string service;
                    if (detected.queryContentLocationSubset())
                        service = detected.queryContentLocationSubset();
                    StringBuffer include;
                    bool found = false;

                    if (m_cmd->inScripts)
                        found = loadScript(path, include);
                    else
                        found = loadEsdl(path, service, include);

                    if (found)
                        return injector.injectContent(include.str(), -1);
                    else
                        throw makeStringExceptionV(-1, "Unable to find Include '%s' with specified search paths.", path.c_str() );
                }

                IncludeLoader(EsdlBuildBindingCmd* cmd) : m_cmd(cmd)
                {}

            private:
                EsdlBuildBindingCmd* m_cmd;

                bool locateFile(std::string& filePath, std::vector<std::string>& includes, Owned<IFile>& foundFile) const
                {
                    foundFile.set(createIFile(filePath.c_str()));

                    if (foundFile->isFile() == fileBool::foundYes)
                        return true;
                    else if (isAbsolutePath(filePath.c_str()))
                        return false;
                    else
                    {
                        for (std::string include : includes)
                        {
                            StringBuffer tryPath(include.c_str());
                            addNonEmptyPathSepChar(tryPath);
                            tryPath.append(filePath.c_str());
                            foundFile.set(createIFile(tryPath));
                            if (foundFile->isFile() == fileBool::foundYes)
                                return true;
                        }
                    }
                    foundFile.clear();
                    return false;
                }

                bool loadScript(std::string& path, StringBuffer& script) const
                {
                    Owned<IFile> file;
                    if (locateFile(path, m_cmd->scriptIncludes, file))
                    {
                        script.loadFile(file);
                        return script.length() > 0;
                    }

                    return false;
                }

                bool loadEsdl(std::string& path, std::string& service, StringBuffer& esdl) const
                {
                    StringBuffer effectiveService(service.c_str());
                    StringBuffer effectiveIncludePath;
                    Owned<IFile> file;

                    if (!isEmptyString(m_cmd->optService.get()))
                        effectiveService.set(m_cmd->optService);

                    for (std::string include : m_cmd->esdlIncludes)
                        effectiveIncludePath.append(include.c_str()).append(ENVSEPCHAR);

                    int effectiveLength = effectiveIncludePath.length();
                    if (effectiveIncludePath.charAt(effectiveLength) == ENVSEPCHAR)
                        effectiveIncludePath.setLength(effectiveLength-1);

                    if (locateFile(path, m_cmd->esdlIncludes, file))
                    {
                        m_cmd->cmdHelper.getServiceESXDL(path.c_str(), effectiveService.str(), esdl, 0, NULL, (DEPFLAG_INCLUDE_TYPES & ~DEPFLAG_INCLUDE_METHOD), effectiveIncludePath.str(), m_cmd->optTraceFlags());
                        return true;
                    }

                    return false;
                }
        };

        inline bool isScriptWrapper(const char* tag)
        {
            return streq(tag, MANIFEST_TAG_SCRIPTS) || streq(tag, MANIFEST_TAG_TRANSFORMS);
        }

        int processScripts(IXmlPullParser* xpp, StartTag root)
        {
            int type;
            StartTag stag;
            EndTag etag;
            // Always called upon seeing a start tag
            int lastType = XmlPullParser::START_TAG;
            // Track the node level nesting of nodes handled in this function only
            int level = 1;

            // Level of parse depth where the ESDL Script entry points appear.
            // Used to identify when we've encountered an entry point root node
            // therefore we need to output the namespace declarations for that
            // entry point. We don't want ns declarations on every start tag.
            int entryPointLevel = 0;

            // Signals which list of include search paths to use
            inScripts = true;

            // To preserve order, the expected format for scripts is:
            // <Scripts>
            //     <![CDATA[<Scripts>...actual scripts...</Scripts>]]>
            // </Scripts>
            copyOutStartTag(xpp, root);
            binding.append("<![CDATA[<Scripts>");

            do
            {
                type = xpp->next();
                switch (type)
                {
                    case XmlPullParser::START_TAG:
                    {
                        xpp->readStartTag(stag);
                        // If the manifest or included file further wraps scripts in a
                        // root node named <Scripts> or <Transforms> then skip it
                        if ( !isScriptWrapper(stag.getLocalName()) )
                        {
                            if (0 == entryPointLevel)
                                entryPointLevel = level;
                            copyOutStartTag(xpp, stag, (level == entryPointLevel));
                        }
                        level++;
                        break;
                    }

                    case XmlPullParser::CONTENT:
                        binding.append(xpp->readContent());
                        break;

                    case XmlPullParser::END_TAG:
                    {
                        xpp->readEndTag(etag);

                        // We're about to encounter the root <Scripts> node, so close
                        // out the CDATA section we injected first
                        if (1 == level)
                            binding.append("</Scripts>]]>\n");

                        // Skip over <Scripts> or <Transforms> nodes wrapping scripts,
                        // but always output the root level <Scripts> end tag
                        if (1 == level || !isScriptWrapper(etag.getLocalName()))
                        {
                            if (XmlPullParser::START_TAG == lastType)
                                binding.insert(binding.length()-1,'/');
                            else
                                binding.appendf("</%s>",etag.getQName());
                        }
                        level--;
                        break;
                    }

                    case XmlPullParser::END_DOCUMENT:
                        level = 0;
                        break;
                }
                lastType = type;
            }
            while (level > 0);

            inScripts = false;
            return type;
        }

        int processTransform(IXmlPullParser* xpp, StartTag root)
        {
            int type;
            StartTag stag;
            EndTag etag;
            // Always called upon seeing a start tag
            int lastType = XmlPullParser::START_TAG;
            // Track the node level nesting of nodes handled in this function only
            int level = 1;

            // Signals which list of include search paths to use
            inScripts = true;

            // To preserve order, the expected format for scripts is:
            // <Transform>
            //     <![CDATA[...actual scripts...]]>
            // </Transform>
            copyOutStartTag(xpp, root);
            binding.append("<![CDATA[");

            do
            {
                type = xpp->next();
                switch (type)
                {
                    case XmlPullParser::START_TAG:
                    {
                        xpp->readStartTag(stag);
                        copyOutStartTag(xpp, stag, 1 == level);
                        level++;
                        break;
                    }

                    case XmlPullParser::CONTENT:
                        binding.append(xpp->readContent());
                        break;

                    case XmlPullParser::END_TAG:
                    {
                        xpp->readEndTag(etag);

                        // We're about to encounter the root <Transform> node, so close
                        // out the CDATA section we injected first
                        if (1 == level)
                            binding.append("]]>\n");

                        if (XmlPullParser::START_TAG == lastType)
                            binding.insert(binding.length()-1,'/');
                        else
                            binding.appendf("</%s>",etag.getQName());

                        level--;
                        break;
                    }

                    case XmlPullParser::END_DOCUMENT:
                        level = 0;
                        break;
                }
                lastType = type;
            }
            while (level > 0);

            inScripts = false;
            return type;
        }

        int processServiceDefinition(IXmlPullParser* xpp, StartTag root)
        {
            int type;
            StartTag stag;
            EndTag etag;
            // Always called upon seeing a start tag
            int lastType = XmlPullParser::START_TAG;
            int level = 1;
            isBundle = true;

            // Eat the root start/end tag and output in the expected bundle format
            binding.append("<Definitions>\n<![CDATA[");

            do
            {
                type = xpp->next();
                switch (type)
                {
                    case XmlPullParser::START_TAG:
                    {
                        xpp->readStartTag(stag);
                        const char* localName = stag.getLocalName();
                        copyOutStartTag(xpp, stag);
                        level++;
                        break;
                    }

                    case XmlPullParser::CONTENT:
                        binding.append(xpp->readContent());
                        break;

                    case XmlPullParser::END_TAG:
                    {
                        xpp->readEndTag(etag);
                        if (!eatRootEndTag(etag, "ServiceDefinition", level))
                        {   if (XmlPullParser::START_TAG == lastType)
                                binding.insert(binding.length()-1,'/');
                            else
                                binding.appendf("</%s>",etag.getQName());
                        }
                        level--;
                        break;
                    }

                    case XmlPullParser::END_DOCUMENT:
                        level = 0;
                        break;
                }
                lastType = type;
            }
            while (level > 0);

            binding.append("]]>\n</Definitions>\n");

            return type;
        }

        int skipNextEndTag(IXmlPullParser* xpp)
        {
            int type;
            do
            {
                type = xpp->next();
            }
            while(type != XmlPullParser::END_TAG && type != XmlPullParser::END_DOCUMENT);
            EndTag etag;
            xpp->readEndTag(etag);

            return type;
        }

        inline bool eatRootEndTag(EndTag etag, const char* match, int level)
        {
            // When we're at the root level (level 1) inside a node, we want
            // to eat an EndTag that matches our match string.
            return (1 == level && streq(etag.getLocalName(), match));
        }

        int processServiceBinding(IXmlPullParser* xpp, StartTag root)
        {
            int type;
            StartTag stag;
            EndTag etag;
            // always called upon seeing a start tag
            int lastType = XmlPullParser::START_TAG;
            // track the node level nesting of nodes handled in this function only
            int level = 1;

            // Eat the root start/end tags and output the expected binding format
            binding.append("<Binding>\n<Definition>\n");

            do
            {
                type = xpp->next();
                switch (type)
                {
                    case XmlPullParser::START_TAG:
                    {
                        xpp->readStartTag(stag);
                        const char* localName = stag.getLocalName();
                        if (streq(localName, MANIFEST_TAG_SCRIPTS))
                            type = processScripts(xpp, stag);
                        else if (streq(localName, MANIFEST_TAG_TRANSFORM))
                            type = processTransform(xpp, stag);
                        else
                        {
                            copyOutStartTag(xpp, stag);
                            level++;
                        }
                        break;
                    }

                    case XmlPullParser::CONTENT:
                        binding.append(xpp->readContent());
                        break;

                    case XmlPullParser::END_TAG:
                    {
                        xpp->readEndTag(etag);
                        if (!eatRootEndTag(etag, "ServiceBinding", level))
                        {
                            if (XmlPullParser::START_TAG == lastType)
                                binding.insert(binding.length()-1,'/');
                            else
                                binding.appendf("</%s>",etag.getQName());
                        }
                        level--;
                        break;
                    }

                    case XmlPullParser::END_DOCUMENT:
                        level = 0;
                        break;
                }

                lastType = type;

            }
            while (level > 0);

            binding.append("</Definition>\n</Binding>\n");
            return type;
        }



        bool processManifest()
        {
            bool result = true;
            try
            {
                manifest.loadFile(optManifestPath.str());
                int size = manifest.length();

                if (size>0)
                {

                    Owned<fxa::IAssistant> assistant;
                    assistant.setown(fxa::createAssistant());
                    assistant->setExternalDetector(fxa::createExternalDetector({{"path", "service", ExternalDetectionType::Path}, {"path", "", ExternalDetectionType::Path}}));
                    assistant->setExternalLoader(new IncludeLoader(this));
                    assistant->registerExternalFragment(MANIFEST_URI, "Include", 0);

                    //std::unique_ptr<XmlPullParser> xpp(new XmlPullParser());
                    std::unique_ptr<fxpp::IFragmentedXmlPullParser> xpp(fxpp::createParser());
                    xpp->setSupportNamespaces(true);
                    xpp->keepWhitespace(true);
                    xpp->setInput(manifest.str(), size);
                    xpp->setAssistant(assistant.getClear());

                    int type;
                    int lastType;
                    StartTag stag;
                    EndTag etag;
                    // track the node level nesting of nodes handled in this function only
                    int level = 1;

                    do { type = xpp->next(); }
                    while (type != XmlPullParser::END_DOCUMENT && type != XmlPullParser::START_TAG);

                    if (XmlPullParser::END_DOCUMENT == type)
                        throw makeStringException(-1, "Manifest file missing root <Manifest> tag");

                    // Eat the start/end tags for <em:Manifest>
                    lastType = type;
                    xpp->readStartTag(stag);

                    const char* localName = stag.getLocalName();
                    const char* uri = stag.getUri();
                    if (isEmptyString(localName) || !streq(localName, MANIFEST_TAG_MANIFEST))
                        throw makeStringException(-1, "Manifest file root tag must be <Manifest>");

                    if (isEmptyString(uri))
                        throw makeStringExceptionV(-1, "Manifest file not using expected namespace '%s'", MANIFEST_URI);

                    if (!streq(uri, MANIFEST_URI))
                        throw makeStringExceptionV(-1, "Manifest file using incorrect namespace URI '%s', must be '%s'", uri, MANIFEST_URI);

                    do
                    {
                        type=xpp->next();
                        if (XmlPullParser::START_TAG == type)
                        {
                            xpp->readStartTag(stag);
                            const char* localName = stag.getLocalName();
                            const char* qname = stag.getQName();
                            const char* uri = stag.getUri();

                            if (streq(localName, "ServiceBinding"))
                                type = processServiceBinding(xpp.get(), stag);
                            else if (streq(localName, "ServiceDefinition"))
                                type = processServiceDefinition(xpp.get(), stag);
                            else
                            {
                                copyOutStartTag(xpp.get(), stag);
                                level++;
                            }
                        }
                        else if (XmlPullParser::CONTENT == type)
                        {
                            binding.append(xpp->readContent());
                        }
                        else if (XmlPullParser::END_TAG == type)
                        {
                            xpp->readEndTag(etag);
                            if(!eatRootEndTag(etag, "Manifest", level))
                            {    if (XmlPullParser::START_TAG == lastType)
                                    binding.insert(binding.length()-1,'/');
                                else
                                    binding.appendf("</%s>",etag.getQName());
                            }
                            level--;
                        }
                        else if (XmlPullParser::END_DOCUMENT == type)
                            level = 0;

                        lastType = type;

                        #ifdef _MANIFEST_DEBUG_
                        //UERRLOG("processBinding while:(...%s)", binding.length()<100 ? binding.str() : binding.str()+binding.length()-100);
                        #endif
                    }
                    while (level > 0);

                    #ifdef _MANIFEST_DEBUG_
                    //UERRLOG("processBinding final:(%s)", binding.str());
                    #endif
                }
                else
                {
                    UERRLOG("Error reading manifest file (%s): empty or does not exist.", optManifestPath.str());
                    result = false;
                }


            }
            catch (IException* e)
            {
                // Use reporting instead...
                StringBuffer msg;
                e->errorMessage(msg);
                UERRLOG("Error reading manifest file (%s): %s", optManifestPath.str(), msg.str());
                result = false;
                e->Release();
            }
            catch (XmlPullParserException* e)
            {
                const char* msg = e->getMessage().c_str();
                UERRLOG("Error reading manifest file (%s) at line #%d, column #%d: %s", optManifestPath.str(), e->getLineNumber(), e->getColumnNumber(), msg ? msg : "Unknown");
            }
            catch (...)
            {
                // Use reporting instead...
                UERRLOG("Unknown error reading manifest file (%s)", optManifestPath.str());
                result = false;
            }

            return result;
        }

        void copyOutStartTag(IXmlPullParser* xpp, StartTag& stag, bool writeNsAttr = false)
        {
            // Attributes can have ns prefixes, but copying
            // those out isn't supported (yet)

            // this new current start tag
            binding.appendf("<%s", stag.getQName());
            for (int i=0; ; i++)
            {
                const char* attr = stag.getRawName(i);
                if (!attr)
                    break;

                // Encode attribute values.
                // Verify if this should always be done.
                // Maybe it needs to be an option, or perhaps
                // only certain attribtues or attribute values
                // should be encoded.
                StringBuffer encoded;
                const char* value = stag.getValue(i);
                if (value && *value)
                    encodeXML(value, encoded);
                binding.appendf(" %s=\"%s\"", attr, encoded.str());
            }

            if (writeNsAttr)
            {
                std::map<std::string, const SXT_CHAR*>::const_iterator it = xpp->getNsBegin();
                while (it != xpp->getNsEnd())
                {
                    if (it->first.compare("xml")!=0)
                        binding.appendf(" xmlns:%s=\"%s\"", it->first.c_str(), it->second);
                    it++;
                }
            }
            binding.append('>');
        }

        bool outputBinding()
        {
            bool result = true;
            try {
                if (optOutputPath.isEmpty())
                {
                    printf("%s", binding.str());
                }
                else
                {
                    Owned<IFile> bindingFile = createIFile(optOutputPath.str());

                    if (bindingFile)
                    {
                        Owned<IFileIO> bindingFileIO = bindingFile->open(IFOcreate);

                        if (bindingFileIO)
                        {
                            size32_t offset = 0;
                            if (isBundle)
                            {
                                bindingFileIO->write(offset, strlen(BUNDLE_START), BUNDLE_START);
                                offset = bindingFileIO->size();
                            }
                            bindingFileIO->write(offset, binding.length(), binding.str());
                            if (isBundle)
                            {
                                offset = bindingFileIO->size();
                                bindingFileIO->write(offset, strlen(BUNDLE_END), BUNDLE_END);
                            }

                        } else {
                            UERRLOG("Error writing to binding file (%s).", optOutputPath.str());
                            result = false;
                        }

                    } else {
                        UERRLOG("Error creating binding file (%s).", optOutputPath.str());
                        result = false;
                    }
                }
            } catch (IException* e) {
                // Use reporting instead...
                StringBuffer msg;
                e->errorMessage(msg);
                UERRLOG("Error writing binding file (%s): %s", optOutputPath.str(), msg.str());
                result = false;
                e->Release();
            } catch (...) {
                // Use reporting instead...
                UERRLOG("Unknown error writing binding file (%s)", optOutputPath.str());
                result = false;
            }
            return result;
        }
};