(
var root = thisProcess.nowExecutingPath.dirname.dirname;
var file = root +/+ "HelpSource/Classes/PinkTrombone.schelp";
var parsed, doc;
try {
    parsed = SCDoc.parseFileFull(file);
    if(parsed.isNil) { Error("Help parser failed").throw };
    doc = SCDoc.parseFileMetaData(root +/+ "HelpSource", "Classes/PinkTrombone.schelp");
    if(doc.isNil) { Error("Help metadata parser failed").throw };
    SCDocHTMLRenderer.renderToFile(root +/+ "build/PinkTrombone-help.html", doc, parsed);
    "PASS: help parsed and rendered".postln;
    0.exit;
} { |error| error.reportError; 1.exit };
)
