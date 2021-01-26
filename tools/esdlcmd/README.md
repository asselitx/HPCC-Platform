# esdl Tool

The esdl tool aids with creating and managing ESDL-based and Dynamic ESDL services on an HPCC cluster. It consists of several different commands.

## build-binding

Creates an xml binding file that specifies a DESDL service. The binding is required to publish a service for use. Two forms of binding exist- the *binding* and the *bundle*. The *binding* has dynamic service configuration, whereas the *bundle* is a wrapper around the binding that adds the ESDL interface definition.

This command takes as input a manifest file. The manifest file specifies a binding *template* and how the resulting binding should be output. The format of the template file dictates if a *binding* or a *bundle* is output.

### Manifest file

The format of a manifest file is below. Future updates will add more capabilities.

    <desdl-manifest>
        <service-template-file path="/full/path/to/ws_foobar-template.xml"/>
        <output write-file="1">
            <file path="/full/path/to/output/ws_foobar-binding.xml"/>
        </output>
    </desdl-manifest>

### Binding Template

Includes these components for a single service:

1. Enumeration of all methods in the service
2. Configuration of the back-end of each method
3. Transform scripts defined in the scope of the service
4. Transform scripts defined in the scope of each method

The binding template allows you to organize your scripts as you see fit in a repo, then use `<include>` tags in the template to include the source of the referenced scripts in the binding output. The format of a binding template is below.

*Note for Tim and Tony- this structure was set early on before Tony's latest changes introducing the use of the CDATA section. It could be changed to more closely reflect the actual output of the binding- instead of Methods/Transforms we could use Methods/Scripts. We may have to consider which versions of the platform we'd want to tool to be able to target (any versions lower than the first version it was introduced?)*

    <Binding>
        <Definition>
            <Methods>
                <Transforms>
                    <include file="/full/path/to/FooBarService-script1.xml"/>
                    <include file="/full/path/to/FooBarService-script2.xml"/>
                </Transforms>
                <Method name="FooBarReport">
                    <Gateways>
                        <Gateway name="baz" url="www.baz.com/api"/>
                    </Gateways>
                    <Transforms>
                        <include file="/full/path/to/FooBarReport-script1.xml"/>
                    </Transforms>
                </Method>
            </Methods>
        </Definition>
    </Binding>

### Bundle Template

Includes all the components of a binding in addition to an ESDL interface definition. The format of a binding looks like:

    <ESDLBundle>
        <Binding>
            <Definition>
                <Methods>
                <Transforms>
                    <include file="/full/path/to/FooBarService-script1.xml"/>
                    <include file="/full/path/to/FooBarService-script2.xml"/>
                </Transforms>
                <Method name="FooBarReport">
                    <Gateways>
                        <Gateway name="baz" url="www.baz.com/api"/>
                    </Gateways>
                    <Transforms>
                        <include file="/full/path/to/FooBarReport-script1.xml"/>
                    </Transforms>
                </Method>
            </Methods>
            </Definition>
        </Binding>
        <Definitions>
            <include file="/full/path/to/ws_foobar.ecm" service="WsFooBar" searchPaths="/path/one:/path/one/two"/>
        </Definitions>
    <ESDLBundle>

### Usage

*Note for Tim and Tony: the `--manifest` option should actually be a required positional argument, I'll make that change with any other changes you suggest*

    Usage:
    esdl build-binding [options]

    Options:
        --manifest <file>   Path to manifest file.
        --no-cdata          Output binding without wrapping scripts in CDATA.
        -I | --include-path <path>
                            Search path for includes referenced in the source file. Can
                            be used multiple times.
        --help              Display usage information for the given command
        -v,--verbose        Output additional tracing information
        -tcat,--trace-category <flags>
                            Control which debug messages are output; a case-insensitive
                            comma-delimited combination of:
                                dev: all output for the developer audience
                                admin: all output for the operator audience
                                user: all output for the user audience
                                err: all error output
                                warn: all warning output
                                prog: all progress output
                                info: all info output
                            Errors and warnings are enabled by default if not verbose,
                            and all are enabled when verbose. Use an empty <flags>
                            value to disable all.

To target platform versions less than version 8, use the --no-cdata flag:

`esdl build-binding --no-cdata --manifest <path/to/manifest.xml>`

Note that the `-I | --include-path` option is not yet implemented. All script include paths in the template must be absolute.

### Output Formats

The esdl build-binding command reads the template specified in the manifest and inserts the included files inline. It will also by default wrap the script and definition sections in CDATA declarations to preserve element order. The output generated for each type of template is shown below.

#### Binding

    <Binding>
        <Definition>
            <Methods>
                <Scripts>
                    <![CDATA[<Transforms>
                        <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="script1">...</CustomRequestTransform>
                        <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="script2">...</CustomRequestTransform>
                    </Transforms>]]>
                </Scripts>
            <Method>
            </Method name="FooBarReport">
                <Scripts>
                    <![CDATA[<Transforms>
                        <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="report-script1">...</CustomRequestTransform>
                    </Transforms>]]>
                </Scripts>
            <Methods>
        </Definition>
    </Binding>

#### Bundle

Includes all the components of a binding in addition to an ESDL interface definition. The format of a binding looks like:

    <ESDLBundle>
        <Binding>
            <Definition>
                <Methods>
                    <Scripts>
                        <![CDATA[<Transforms>
                            <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="script1">...</CustomRequestTransform>
                        <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="script2">...</CustomRequestTransform>
                        </Transforms>]]>
                    </Scripts>
                <Method>
                </Method name="FooBarReport">
                    <Scripts>
                        <![CDATA[<Transforms>
                            <es:CustomRequestTransform xmlns:es="urn:hpcc:esdl:script" name="report-script1">...</CustomRequestTransform>
                        </Transforms>]]>
                    </Scripts>
                <Methods>
            </Definition>
        </Binding>
        <Definitions>
            <![CDATA[<esxdl name="WsServiceName">...</esxdl>]]>
        </Definitions>
    <ESDLBundle>
