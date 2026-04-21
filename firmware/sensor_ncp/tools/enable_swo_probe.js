const { join } = require("path");

const holdMs = Number(process.argv[3] || "15000");
const doReset = process.argv.includes("--reset");

const scriptingOptions = process.env.CCS_ROOT ? { ccsRoot: process.env.CCS_ROOT } : undefined;
const ds = initScripting(scriptingOptions);
ds.setScriptingTimeout(30000);

const ccxml = process.env.CCXML_PATH || join(__dirname, "cc1352p7_2pin_cJTAG_XDS110.ccxml");
console.log(`Configuring debugger with ${ccxml}`);
ds.configure(ccxml);

const cores = ds.listCores();
console.log(`Cores: ${JSON.stringify(cores)}`);

let session = null;
if (cores.cores && cores.cores.length > 0) {
    session = ds.openSession(/cortex/i);
    session.target.connect();
    console.log("Connected to target");
}
else {
    console.log("No debug core exposed by this probe profile; holding probe in configured mode");
}

if (session && doReset) {
    const resets = session.target.getResets();
    const resetType = resets["System Reset"] ? "System Reset" : "";
    console.log(`Issuing ${resetType || "default"} reset`);
    session.target.reset(resetType);
}

if (session && session.target.isHalted()) {
    console.log("Target is halted, resuming");
    session.target.run(false);
}

console.log(`Holding session open for ${holdMs} ms`);
sleep(holdMs);

if (session && session.target.isConnected()) {
    session.target.disconnect();
}
ds.shutdown();
console.log("Debugger session closed");
