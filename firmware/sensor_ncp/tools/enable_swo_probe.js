importPackage(Packages.com.ti.ccstudio.scripting.environment);
importPackage(Packages.java.io);
importPackage(Packages.java.lang);

function argOrDefault(index, defaultValue)
{
    if (typeof arguments !== "undefined" && arguments.length > index)
    {
        var value = arguments[index];
        if (value !== null && String(value).length > 0)
        {
            return value;
        }
    }

    return defaultValue;
}

function stringContains(value, needle)
{
    return String(value).toLowerCase().indexOf(String(needle).toLowerCase()) !== -1;
}

var defaultHoldMs = 86400000;
var holdMs = Number(argOrDefault(0, String(defaultHoldMs)));
if (!(holdMs > 0))
{
    holdMs = defaultHoldMs;
}
var doReset = String(argOrDefault(1, "")).toLowerCase() === "--reset";
var ccxmlArg = argOrDefault(2, "");
var userDir = String(System.getProperty("user.dir"));
var ccxmlPath = ccxmlArg ? String(ccxmlArg) : String(new File(userDir, "tools/cc1352p7_2pin_cJTAG_XDS110.ccxml").getCanonicalPath());

var script = ScriptingEnvironment.instance();
script.setScriptTimeout(Math.max(30000, holdMs + 5000));
script.traceBegin("swo_probe_trace.xml", "DefaultStylesheet.xsl");
script.traceSetConsoleLevel(TraceLevel.INFO);

print("Configuring debugger with " + ccxmlPath);

var debugServer = script.getServer("DebugServer.1");
debugServer.setConfig(ccxmlPath);

var session = null;

try
{
    try
    {
        session = debugServer.openSession(".*Cortex_M4.*");
    }
    catch (openErr)
    {
        session = debugServer.openSession(".*");
    }

    if (session)
    {
        session.target.connect();
        print("Connected to target");

        if (doReset)
        {
            try
            {
                var resets = session.target.getResets();
                var resetType = "";

                if (resets)
                {
                    for (var key in resets)
                    {
                        if (stringContains(key, "system reset"))
                        {
                            resetType = key;
                            break;
                        }
                    }
                }

                print("Issuing " + (resetType ? resetType : "default") + " reset");
                session.target.reset(resetType);
            }
            catch (resetErr)
            {
                print("Reset failed: " + resetErr);
            }
        }

        try
        {
            if (session.target.isHalted())
            {
                print("Target is halted, resuming");
                session.target.runAsynch();
            }
        }
        catch (runErr)
        {
            print("Resume check failed: " + runErr);
        }
    }
    else
    {
        print("No debug session opened");
    }

    print("Holding session open for " + holdMs + " ms");
    Thread.sleep(holdMs);
}
finally
{
    try
    {
        if (session)
        {
            session.target.disconnect();
        }
    }
    catch (disconnectErr)
    {
        print("Disconnect failed: " + disconnectErr);
    }

    try
    {
        debugServer.stop();
    }
    catch (stopErr)
    {
        print("Debug server stop failed: " + stopErr);
    }

    script.traceEnd();
}

print("Debugger session closed");
