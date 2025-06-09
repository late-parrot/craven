from pathlib import Path
import subprocess

RED = "\033[31m"
BLU = "\033[34m"
GRN = "\033[32m"
YLW = "\033[33m"
RST = "\033[0m"

base_dir = Path(".")
test_dir = base_dir / "test" / "unittest"
lang_bin = base_dir / "build" / "craven.out"
glob = "*.rvn"

passed = 0
failed = 0
skipped = 0

for f in test_dir.glob(glob):
    print(f"\r{GRN}{passed}{RST} Passed, {RED}{failed}{RST} Failed, {YLW}{skipped}{RST} Skipped.\r", end="")

    code = f.read_text()
    if "// UNITTEST" not in code:
        skipped += 1
        continue
    config = code.split("// UNITTEST")[1].strip()
    options = {
        x.split(":")[0].lstrip("/").strip() : x.split(":")[1].strip().replace("\\n", "\n")
        for x in config.splitlines()
        if x.strip() != ""
    }
    name = options.get("name", f.name)
    expected = options.get("expected", "")
    error = options.get("error", "")
    status = options.get("status")
    skip = options.get("skip", "no")
    
    if skip == "yes":
        skipped += 1
        continue
    out = subprocess.run([lang_bin, f], capture_output=True)
    stdout, stderr, exitcode = out.stdout.strip().decode(), out.stderr.strip().decode(), out.returncode
    if stderr != error:
        failed += 1
        stderr = "\n".join(["    "+x for x in stderr.splitlines()]) if stderr else "    (nothing)"
        error = "\n".join(["    "+x for x in error.splitlines()]) if error else "    (nothing)"
        print(f"{RED}✘ Test '{name}' failed. Expected stderr:\n{BLU}{error}{RED}\n"+
              f"  Recieved this instead:\n{BLU}{stderr}{RST}")
        continue
    if stdout != expected:
        failed += 1
        stdout = "\n".join(["    "+x for x in stdout.splitlines()]) if stdout else "    (nothing)"
        expected = "\n".join(["    "+x for x in expected.splitlines()]) if expected else "    (nothing)"
        print(f"{RED}✘ Test '{name}' failed. Expected stdout:\n{BLU}{expected}{RED}\n"+
              f"  Recieved this instead:\n{BLU}{stdout}{RST}")
        continue
    if status is not None and exitcode != status:
        failed += 1
        print(f"{RED}✘ Test '{name}' failed. Expected exit code {BLU}{status}{RED} but got {BLU}{exitcode}{RED} instead")
        continue
    passed += 1

print(f"\r{GRN}{passed}{RST} Passed, {RED}{failed}{RST} Failed, {YLW}{skipped}{RST} Skipped.")