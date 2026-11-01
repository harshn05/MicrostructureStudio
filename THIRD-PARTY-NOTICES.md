# Third-party software notices

Microstructure Studio uses third-party software. This file is an inventory and
release checklist, not a substitute for the licence texts and notices shipped
by those projects.

| Component | Used by | Licence / notice source |
|---|---|---|
| Avalonia UI | Managed UI framework and controls | MIT; https://github.com/AvaloniaUI/Avalonia |
| .NET 10 runtime | Self-contained managed application runtime | Microsoft .NET licence and third-party notices; retain the files supplied with the installed .NET runtime |
| SkiaSharp | Avalonia rendering backend dependency | MIT; https://github.com/mono/SkiaSharp |
| HarfBuzzSharp | Text shaping dependency | MIT; https://github.com/mono/SkiaSharp |
| MicroCom.Runtime | Avalonia native interop dependency | Check the package's licence metadata and retain its notice |
| Other NuGet transitive dependencies | Resolved during restore/publish | Check each resolved package's licence metadata |

## Release checklist

Before publishing a binary release:

1. Generate a fresh Release publish from the exact source revision.
2. Inspect the resolved NuGet package graph (`dotnet list managed/MicrostructureStudio.csproj package --include-transitive`).
3. Verify the licence of every direct and transitive package actually included in the publish output.
4. Include the applicable full licence texts and copyright notices in the release archive. Do not rely on this inventory alone.
5. Retain the .NET runtime licence and `ThirdPartyNotices.txt` / equivalent notice files supplied by the .NET distribution. Locate these in the installed runtime/SDK or the official .NET distribution for the exact version being shipped.
6. Do not remove notices when trimming or packaging the application.

The list above is based on the current project references and observed publish output.
Dependency versions and transitive packages can change. Re-audit every release.
