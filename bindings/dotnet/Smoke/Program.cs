using OpenQR;

var payload = new byte[] { 32, 65, 0, 66, 10, 32 };
var batch = Engine.Generate(payload, options: new Options { Version = 3, Scale = 3, Seed = 42 });
var entry = batch.Results.Single();
if (!entry.Payload.SequenceEqual(payload) || batch.Seed != 42 || entry.VerifiedFrames != 1 || entry.TotalFrames != 1 || entry.Version != 3 || entry.Image.Length < 8 || !entry.Image.AsSpan(0, 8).SequenceEqual(new byte[] { 137, 80, 78, 71, 13, 10, 26, 10 }))
    throw new InvalidOperationException("The managed/native result does not match the request.");
if (entry.BlockErrors.Length != entry.BlockCapacities.Length || entry.BlockErrors.Any(error => error != 0))
    throw new InvalidOperationException("Invalid Reed-Solomon metadata.");
for (var category = 0; category < 3; ++category)
{
    var styles = Engine.Styles(category);
    if (styles.Count == 0) throw new InvalidOperationException("Missing native style catalog.");
    var preview = System.Xml.Linq.XElement.Parse(System.Text.Encoding.UTF8.GetString(Engine.StylePreview(category, styles.Keys.First())));
    if (preview.Name.LocalName != "svg") throw new InvalidOperationException("Invalid native style preview.");
}
try
{
    Engine.Generate(payload, options: new Options { Version = 3, Scale = 4 });
    throw new InvalidOperationException("The native option error was lost.");
}
catch (OpenQRException error) when (error.Status == 1) { }
Console.WriteLine($"OpenQR {Engine.Version}: .NET ABI smoke passed");
