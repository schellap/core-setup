using System.Collections.Generic;

namespace Microsoft.DotNet.Cli.Build
{
    public class HostVersion : Version
    {
        // ------------------------------------------HOST-VERSIONING-------------------------------------------
        //
        // Host versions are independent of CLI versions. Moreover, these version numbers
        // are baked into the binary and is used to look up a serviced binary replacement.
        //

        public struct VerInfo
        {
            public int Major;
            public int Minor;
            public int Patch;
            public string Release;
            public string BuildMajor;
            public string BuildMinor;
            public string CommitCountString;

            public VerInfo(int major, int minor, int patch, string release, string buildMajor, string buildMinor, string commitCountString)
            {
                Major = major;
                Minor = minor;
                Patch = patch;
                Release = release;
                BuildMajor = buildMajor;
                BuildMinor = buildMinor;
                CommitCountString = commitCountString;
            }

            public string WithoutSuffix => $"{Major}.{Minor}.{Patch}";

            public string MsiBuildMajor => string.IsNullOrEmpty(BuildMajor) ? BuildMajor : CommitCountString;
            public string MsiBuildMinor => string.IsNullOrEmpty(BuildMinor) ? BuildMinor : "00";

            public override string ToString()
            {
                string suffix = "";
                foreach (var verPad in new string[] { Release, BuildMajor, BuildMinor })
                {
                    if (!string.IsNullOrEmpty(verPad))
                    {
                        suffix += $"-{verPad}";
                    }
                }
                return $"{Major}.{Minor}.{Patch}{suffix}";
            }
        }
        //
        // Latest hosts for production of nupkgs.
        //

        // Full versions and package information.
        public bool EnsureStableVersion => true;
        public string LatestHostPrerelease => "rc3";
        public string LatestHostBuildMajor => "";
        public string LatestHostBuildMinor => "";
        public VerInfo LatestHostVersion => new VerInfo(1, 0, 1, "", "", "", CommitCountString);
        public VerInfo LatestHostFxrVersion => new VerInfo(1, 0, 1, "", "", "", CommitCountString);
        public VerInfo LatestHostPolicyVersion => new VerInfo(1, 0, 1, "", "", "", CommitCountString);
        public Dictionary<string, VerInfo> LatestHostPackages => new Dictionary<string, VerInfo>()
        {
            { "Microsoft.NETCore.DotNetHost", LatestHostVersion },
            { "Microsoft.NETCore.DotNetHostResolver", LatestHostFxrVersion },
            { "Microsoft.NETCore.DotNetHostPolicy", LatestHostPolicyVersion }
        };
        public Dictionary<string, VerInfo> LatestHostBinaries => new Dictionary<string, VerInfo>()
        {
            { "dotnet", LatestHostVersion },
            { "hostfxr", LatestHostFxrVersion },
            { "hostpolicy", LatestHostPolicyVersion }
        };

        //
        // Locked muxer for consumption in CLI.
        //
        public bool IsLocked = false; // Set this variable to toggle muxer locking.
        public VerInfo LockedHostFxrVersion => IsLocked ? new VerInfo(1, 0, 1, "rc2", "002468", "00", "002468") : LatestHostFxrVersion;
        public VerInfo LockedHostVersion    => IsLocked ? new VerInfo(1, 0, 1, "rc2", "002468", "00", "002468") : LatestHostVersion;
    }
}
