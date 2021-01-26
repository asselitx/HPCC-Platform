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
#include <xpp/XmlPullParser.h>
#include <memory>
#include <unordered_set>

using namespace xpp;

//#define _MANIFEST_DEBUG_

#define ESDL_BUILD_BINDING_ERR  23

// String constants for element names in the manifest or template file
#define MANIFEST_TAG_BIND_TEMPLATE  "service-template-file"
#define MANIFEST_TAG_OUTPUT         "output"
#define MANIFEST_TAG_OUTPUT_FILE    "file"

#define TEMPLATE_TAG_TRANSFORM      "Transform"
#define TEMPLATE_TAG_TRANSFORMS     "Transforms"
#define TEMPLATE_TAG_CRT            "CustomRequestTransform"
#define TEMPLATE_TAG_PRELOG         "PreLogging"
#define TEMPLATE_TAG_BACKEND_REQ    "BackendRequest"
#define TEMPLATE_TAG_BACKEND_RESP   "BackendResponse"
#define TEMPLATE_TAG_ESDL_DEFNS     "Definitions"

class EsdlBuildBindingCmd : public EsdlCmdCommon
{
    protected:
        // Command-line options
        StringAttr optManifestPath;
        bool optNoCdata = false;
        StringAttr optService;
        StringBuffer optIncludePath;

        // Manifest file as PTree
        Owned<IPropertyTree> manifest;

        // Service binding template
        StringBuffer bindTemplate;

        // Binding output
        StringBuffer binding;

        // Parsed settings from the manifest file
        bool outputWrite;
        bool outputPublish;
        StringAttr outputPath;
        StringAttr bindTemplatePath;

        // List of allowed tag names for Transform elements
        std::unordered_set<std::string> transformTags{TEMPLATE_TAG_TRANSFORM, TEMPLATE_TAG_CRT, TEMPLATE_TAG_PRELOG, TEMPLATE_TAG_BACKEND_REQ, TEMPLATE_TAG_BACKEND_RESP};

        EsdlCmdHelper cmdHelper;

    public:

        EsdlBuildBindingCmd() : outputWrite(false), outputPublish(false)
        {

        }

        int processCMD() override
        {   bool result = false;

            try
            {
                printf("Manifest: %s\n", optManifestPath.str());

                result = readManifest();

                #ifdef _MANIFEST_DEBUG_
                if (result)
                {
                    StringBuffer tmp;
                    toXML(manifest, tmp);
                    DBGLOG("Manifest contents: (%s)", tmp.str());
                }
                #endif

                if (!result)
                {
                    UERRLOG("Error: Failed reading manifest. Exiting.");
                    return result;
                }
                result = parseManifest();

                if (!result)
                {
                    UERRLOG("Error: Failed parsing manifest. Exiting.");
                    return result;
                }

                result = processBindingTemplate();

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
                for (; !iter.done(); iter.next())
                {
                    if (iter.matchOption(optManifestPath, ESDLOPT_MANIFEST_PATH))
                        result = true;
                    if (iter.matchFlag(optNoCdata, ESDLOPT_MANIFEST_NOCDATA))
                        result = true;
                    StringAttr oneOption;
                    if (iter.matchOption(oneOption, ESDLOPT_INCLUDE_PATH) || iter.matchOption(oneOption, ESDLOPT_INCLUDE_PATH_S))
                    {
                        if(optIncludePath.length() > 0)
                            optIncludePath.append(ENVSEPSTR);
                        optIncludePath.append(oneOption.get());
                        result = true;
                    }
                    if (iter.matchOption(optService, ESDLOPT_SERVICE))
                        result = true;
                }
            }
            return result;
        }

        void usage() override
        {
            puts("Usage:");
            puts("esdl build-binding [options]\n");
            puts("Options:");
            puts("    --manifest <file>    Path to manifest file.");
            puts("    --no-cdata           Output binding without wrapping scripts in CDATA.");
            puts("    -I | --include-path <path>");
            puts("                         Search path for includes referenced in the source file. Can");
            puts("                         be used multiple times.");
            EsdlCmdCommon::usage();
        }

        bool finalizeOptions(IProperties* globals) override
        {
            return true;
        }

    private:

        // Read the manifest file from the path provided on the command line
        // Parse contents into member PTree manifest
        bool readManifest()
        {
            bool result = true;
            try {
                Owned<IFile> manifestFile = createIFile(optManifestPath.str());

                if (manifestFile->exists() && (manifestFile->isFile() == fileBool::foundYes) && manifestFile->size()>0){
                    Owned<IFileIO> manifestFileIO = manifestFile->open(IFOread);

                    if (manifestFileIO.get())
                    {
                        manifest.setown(createPTree(*manifestFileIO));
                        if (!manifest.get())
                        {
                            UERRLOG("Error reading manifest file (%s): can't create PTree.", optManifestPath.str());
                            result = false;
                        }

                    } else {
                        UERRLOG("Error reading manifest file (%s): can't open.", optManifestPath.str());
                        result = false;
                    }

                } else {
                    UERRLOG("Error reading manifest file (%s): empty or does not exist.", optManifestPath.str());
                    result = false;
                }

            } catch (IException* e) {
                // Use reporting instead...
                StringBuffer msg;
                e->errorMessage(msg);
                UERRLOG("Error reading manifest file (%s): %s", optManifestPath.str(), msg.str());
                result = false;
                e->Release();
            } catch (...) {
                // Use reporting instead...
                UERRLOG("Unknown error reading manifest file (%s)", optManifestPath.str());
                result = false;
            }

            return result;
        }

        // Extract some commonly used elements out of the manifest PTree
        // into member variables
        bool parseManifest()
        {
            StringBuffer xpath;
            xpath.appendf("%s/@write-file", MANIFEST_TAG_OUTPUT);
            outputWrite = manifest->getPropBool(xpath.str());
            #ifdef _MANIFEST_DEBUG_
            DBGLOG("write-file %d", outputWrite);
            #endif

            if (outputWrite)
            {
                xpath.clear().appendf("%s/%s/@path", MANIFEST_TAG_OUTPUT, MANIFEST_TAG_OUTPUT_FILE);
                const char* prop = manifest->queryProp(xpath.str());
                if (!prop)
                {
                    UERRLOG("Error parsing manifest: write-file=true, but no file output element (%s/%s) defined.", MANIFEST_TAG_OUTPUT, MANIFEST_TAG_OUTPUT_FILE);
                    return false;
                }

                outputPath.set(prop);
                #ifdef _MANIFEST_DEBUG_
                DBGLOG("%s = %s", xpath.str(), outputPath.str());
                #endif
            }

            xpath.clear().appendf("%s/@path", MANIFEST_TAG_BIND_TEMPLATE);
            const char* path = manifest->queryProp(xpath.str());
            if (!path)
            {
                UERRLOG("Error parsing manifest: no binding template element (%s) defined.", xpath.str());
                return false;
            }

            bindTemplatePath.set(path);
            #ifdef _MANIFEST_DEBUG_
            DBGLOG("%s = %s", xpath.str(), bindTemplatePath.str());
            #endif

            return true;
        }

        void copyOutStartTag(XmlPullParser* xpp, StartTag& stag, int& lastType, bool writeNsAttr = false)
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
                map< string, const SXT_CHAR* >::iterator it = xpp->getNsBegin();
                while (it != xpp->getNsEnd())
                {
                    if (it->first.compare("xml")!=0)
                        binding.appendf(" xmlns:%s=\"%s\"", it->first.c_str(), it->second);
                    it++;
                }
            }

            lastType = XmlPullParser::START_TAG;
        }

        bool isTransformTag(const char* tagname)
        {
            if( transformTags.find(tagname) != transformTags.end() )
                return true;
            else
                return false;
        }

        bool copyOutTransform(XmlPullParser* xpp, StartTag& stagi, int& lastType)
        {
            bool result = false;

            int type;
            StartTag stag;
            EndTag etag;

            // Copy out the start tag of the transform element passed in
            // Here we assume that the start tag only of a transform will
            // have a namespace definition.
            copyOutStartTag(xpp, stagi, lastType, true);

            // Copy the transform through to the output
            while ((type=xpp->next()) != XmlPullParser::END_DOCUMENT)
            {
                if (XmlPullParser::START_TAG == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    xpp->readStartTag(stag);

                    copyOutStartTag(xpp, stag, lastType);
                }
                else if (XmlPullParser::CONTENT == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    binding.append(xpp->readContent());
                }
                else if (XmlPullParser::END_TAG == type)
                {
                    xpp->readEndTag(etag);
                    // For tags with no children use the <tag ... /> format
                    if (XmlPullParser::START_TAG == lastType)
                        binding.append("/>");
                    else
                        binding.appendf("</%s>", etag.getQName());

                    // If we've reached the end of this transform, then exit
                    if(isTransformTag(etag.getLocalName()))
                    {
                        // newline to separate the transforms
                        // maybe this should be optional or removed?
                        binding.append("\n");
                        result = true;
                        break;
                    }
                }
                lastType=type;
                #ifdef _MANIFEST_DEBUG_
                UERRLOG("copyOutTransform:(%s)", binding.length()<100 ? binding.str() : binding.str()+binding.length()-100);
                #endif
            }

            return result;
        }


        bool loadInclude(XmlPullParser* xpp, StartTag& stag, int& lastType)
        {
            bool result = false;
            const char* path = stag.getValue("file");

            if (!path)
            {
                const char* thisTag = stag.toString().c_str();
                throw MakeStringException(ESDL_BUILD_BINDING_ERR, "Failed loading include. Binding template missing @file attribute in element:%s, file line:%d column:%d.", thisTag, xpp->getLineNumber(), xpp->getColumnNumber());
            }

            // For now load entire file
            // In future, extract @transform name and load just that transform

            // Also in the future we will need to analyze the path here before loading
            //  - determine if it is a complete path to file
            //  - if so, load it
            //  - if not, use the include paths from the manifest to find the file.
            //
            StringBuffer include;
            include.loadFile(path);

            if (include.length()<1)
            {
                throw MakeStringException(ESDL_BUILD_BINDING_ERR, "Failed loading empty include file (%s). Included from binding template at file line:%d column:%d.", path, xpp->getLineNumber(), xpp->getColumnNumber());
            }

            std::unique_ptr<XmlPullParser> xppi(new XmlPullParser());
            xppi->setSupportNamespaces(true);
            xppi->setInput(include.str(), include.length());

            int type;
            int lastTypei=lastType;
            StartTag stagi;

            // Copy the transforms from the include file to the binding
            while ((type=xppi->next()) != XmlPullParser::END_DOCUMENT)
            {
                if (XmlPullParser::START_TAG == type)
                {
                    xppi->readStartTag(stagi);
                    // Use the localName here so that we don't need to
                    // be tied to a specific namespace
                    if(isTransformTag(stagi.getLocalName()))
                    {
                        result = copyOutTransform(xppi.get(), stagi, lastTypei);
                    }
                }
            }
            // Propagate the last type seen when copying out the transform
            // to the parent loop. Should be an END_TAG.
            lastType=lastTypei;

            // Eat the end tag of the <include> element
            xpp->next();

            if (false==result)
                UWARNLOG("No transforms found in file %s.", path);

            return result;
        }

        bool outputScriptSection(XmlPullParser* xpp, StartTag& stag, int& lastType)
        {
            if(!optNoCdata)
                binding.append("<Scripts>");
            binding.append("<![CDATA[");

            copyOutStartTag(xpp, stag, lastType); // <Transforms>

            int type;
            //StartTag stag;
            EndTag etag;
            bool done = false;
            bool result = true;

            // Copy the document through to the output, replacing import statements
            // with the transforms from the referenced files
            while ((type=xpp->next()) != XmlPullParser::END_DOCUMENT && !done)
            {
                if (XmlPullParser::START_TAG == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    xpp->readStartTag(stag);
                    // Use the localName here so that we don't need to
                    // be tied to a specific namespace
                    const char* lname = stag.getLocalName();
                    if (strieq(lname, "include"))
                    {
                        result = loadInclude(xpp, stag, lastType);
                    }
                    else
                    {   // assume it is transform content inline, we could be more strict here
                        copyOutStartTag(xpp, stag, lastType);
                    }
                }
                else if (XmlPullParser::CONTENT == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    binding.append(xpp->readContent());
                    lastType=type;
                }
                else if (XmlPullParser::END_TAG == type)
                {
                    xpp->readEndTag(etag);
                    // For tags with no children use the <tag ... /> format
                    if (XmlPullParser::START_TAG == lastType)
                        binding.append("/>");
                    else
                        binding.appendf("</%s>", etag.getQName());

                    if (strieq(etag.getLocalName(), "Transforms"))
                        done = true;

                    lastType=type;
                }
            }

            binding.append("]]>");
            if(!optNoCdata)
                binding.append("</Scripts>");

            return result;
        }

        bool outputESDLInterfaceDefinitions(XmlPullParser* xpp, StartTag& stag, int& lastType)
        {
            bool result = true;
            int type;
            EndTag etag;
            bool done = false;

            binding.appendf("<%s><![CDATA[", TEMPLATE_TAG_ESDL_DEFNS);

            // Copy the document through to the output, replacing import statements
            // with the transforms from the referenced files
            while ((type=xpp->next()) != XmlPullParser::END_DOCUMENT && !done)
            {
                if (XmlPullParser::START_TAG == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    xpp->readStartTag(stag);
                    // Use the localName here so that we don't need to
                    // be tied to a specific namespace
                    const char* lname = stag.getLocalName();
                    if (strieq(lname, "include"))
                    {
                        const char* file = stag.getValue("file");

                        if (!file)
                        {
                            const char* thisTag = stag.toString().c_str();
                            throw MakeStringException(ESDL_BUILD_BINDING_ERR, "Failed loading definition include. Bundle template missing @file attribute in element:%s, file line:%d column:%d.", thisTag, xpp->getLineNumber(), xpp->getColumnNumber());
                        }

                        StringBuffer esxml;
                        StringBuffer effectiveService(optService.get());
                        StringBuffer effectiveIncludePath(optIncludePath.str());
                        const char* service = stag.getValue("service");
                        const char* includes = stag.getValue("searchPaths");

                        // When command-line options aren't populated, use what may have been
                        // provided as attributes in the <include> tag
                        if (isEmptyString(optService.get()))
                            effectiveService.set(service);
                        if (isEmptyString(optIncludePath.str()))
                            effectiveIncludePath.set(includes);

                        cmdHelper.getServiceESXDL(file, effectiveService.str(), esxml, 0, NULL, (DEPFLAG_INCLUDE_TYPES & ~DEPFLAG_INCLUDE_METHOD), effectiveIncludePath.str(), optTraceFlags());
                        if (esxml.length()==0)
                        {
                            throw MakeStringException(ESDL_BUILD_BINDING_ERR, "ESDL Definition for service %s could not be loaded from: %s", effectiveService.str(), file);
                        }
                        binding.append(esxml);
                    }
                    else
                    {
                        // allow any other content- copy through
                        copyOutStartTag(xpp, stag, lastType);
                    }
                }
                else if (XmlPullParser::CONTENT == type)
                {
                    // close off the last start tag
                    if (XmlPullParser::START_TAG==lastType)
                        binding.append(">");

                    binding.append(xpp->readContent());
                    lastType=type;
                }
                else if (XmlPullParser::END_TAG == type)
                {
                    xpp->readEndTag(etag);
                    if (strieq(etag.getLocalName(), TEMPLATE_TAG_ESDL_DEFNS))
                    {
                        binding.append("]]>");
                        done = true;
                    }

                    // Eat the end tag of the include
                    if (strieq(etag.getLocalName(), "include"))
                        continue;

                    // For tags with no children use the <tag ... /> format
                    if (XmlPullParser::START_TAG == lastType)
                        binding.append("/>");
                    else
                        binding.appendf("</%s>", etag.getQName());
                    lastType=type;
                }
            }

            return result;
        }

        // The binding template outlines what the binding will look
        // like. Here we generate the final binding by loading and
        // printing out any included templates inline.

        bool processBindingTemplate()
        {
            bool result = true;

            bindTemplate.loadFile(bindTemplatePath.str());
            int size = bindTemplate.length();

            if (size>0)
            {
                std::unique_ptr<XmlPullParser> xpp(new XmlPullParser());
                xpp->setSupportNamespaces(true);
                xpp->setInput(bindTemplate.str(), size);

                int type;
                int lastType = 0;
                StartTag stag;
                EndTag etag;

                // Copy the document through to the output, replacing import statements
                // with the transforms from the referenced files
                while ((type=xpp->next()) != XmlPullParser::END_DOCUMENT)
                {
                    if (XmlPullParser::START_TAG == type)
                    {
                        // close off the last start tag
                        if (XmlPullParser::START_TAG == lastType)
                            binding.append(">");

                        xpp->readStartTag(stag);
                        // Use the localName here so that we don't need to
                        // be tied to a specific namespace
                        if (strieq(stag.getLocalName(), TEMPLATE_TAG_TRANSFORMS)) {
                            result = outputScriptSection(xpp.get(), stag, lastType);
                        } else if (strieq(stag.getLocalName(), TEMPLATE_TAG_ESDL_DEFNS)) {
                            result = outputESDLInterfaceDefinitions(xpp.get(), stag, lastType);
                        } else {
                            copyOutStartTag(xpp.get(), stag, lastType);
                            lastType=type;
                        }
                        // lastType is updated by reference by the functions called here
                    }
                    else if (XmlPullParser::CONTENT == type)
                    {
                        // close off the last start tag
                        if (XmlPullParser::START_TAG == lastType)
                            binding.append(">");

                        binding.append(xpp->readContent());
                        lastType=type;
                    }
                    else if (XmlPullParser::END_TAG == type)
                    {
                        xpp->readEndTag(etag);
                        // For tags with no children use the <tag ... /> format
                        if (XmlPullParser::START_TAG == lastType)
                            binding.append("/>");
                        else
                            binding.appendf("</%s>", etag.getQName());

                        lastType=type;
                    }
                    #ifdef _MANIFEST_DEBUG_
                    //UERRLOG("processBinding while:(...%s)", binding.length()<100 ? binding.str() : binding.str()+binding.length()-100);
                    #endif
                }
            }
            #ifdef _MANIFEST_DEBUG_
            //UERRLOG("processBinding final:(%s)", binding.str());
            #endif
            return result;
        }

    bool outputBinding()
    {
        bool result = true;
        try {
            Owned<IFile> bindingFile = createIFile(outputPath.str());

            if (bindingFile)
            {
                Owned<IFileIO> bindingFileIO = bindingFile->open(IFOcreate);

                if (bindingFileIO)
                {
                    bindingFileIO->write(0, binding.length(), binding.str());
                } else {
                    UERRLOG("Error writing to binding file (%s).", outputPath.str());
                    result = false;
                }

            } else {
                UERRLOG("Error creating binding file (%s).", outputPath.str());
                result = false;
            }

        } catch (IException* e) {
            // Use reporting instead...
            StringBuffer msg;
            e->errorMessage(msg);
            UERRLOG("Error writing binding file (%s): %s", outputPath.str(), msg.str());
            result = false;
            e->Release();
        } catch (...) {
            // Use reporting instead...
            UERRLOG("Unknown error writing binding file (%s)", outputPath.str());
            result = false;
        }
        return result;
    }
};