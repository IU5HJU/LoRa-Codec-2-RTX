# Script di PlatformIO (pre): scrive in LCR_VERSION la versione del firmware ricavata da git.
#   tag v0.1.0            -> "v0.1.0"
#   3 commit dopo il tag  -> "v0.1.0-3-gabc1234"
#   senza tag             -> "abc1234" (e "-dirty" se ci sono modifiche non salvate)
#   senza git             -> "dev"
Import("env")  # noqa: F821  (fornito da PlatformIO)
import subprocess

try:
    version = subprocess.check_output(
        ["git", "describe", "--tags", "--always", "--dirty"], stderr=subprocess.DEVNULL).decode().strip()
except Exception:
    version = "dev"
env.Append(CPPDEFINES=[("LCR_VERSION", env.StringifyMacro(version))])  # noqa: F821
print("Versione del firmware: " + version)
