import sys
import os
import subprocess
from pathlib import Path


def input_tty(prompt=""):
    try:
        with open("/dev/tty", "r") as tty_in:
            sys.stdout.write(prompt)
            sys.stdout.flush()
            return tty_in.readline().strip()
    except FileNotFoundError:
        sys.exit("Cannot access terminal input (/dev/tty). Please download install.py from GitHub and run it normally.")

def abort(msg="Installation halted by user, aborting..."):
    print(msg)
    sys.exit()

def ask_option(prompt, *options):
    print(prompt)
    for i, o in enumerate(options):
        print(f"  {i+1}. {o}" + (" (default)" if i==0 else ""))
    n = len(options)
    try:
        response = input_tty(f"Enter for default or q to abort: ").strip()
        while response not in {str(i+1) for i in range(n)} and response != "":
            if response in {"q", "Q"}:
                print()
                abort()
            print(f"Invalid response. Please provide a single number 1-{n}.")
            response = input_tty(f"Enter for default or q to abort: ").strip()
    except EOFError:
        print("\n")
        abort()
    print()
    return 1 if response == "" else int(response)

def skippable_input(prompt, default=""):
    print(prompt)
    response = input_tty("Enter to skip or q to abort: ").strip()
    print()
    if response == "q": abort()
    return response if response != "" else default

def run(*args, **kwargs):
    result = subprocess.run(*args, **kwargs)
    if result.returncode != 0:
        abort("\nError detected. Aborting...")
    return result


def main():
    print("\n".join([x.strip() for x in """
        Welcome to the Raven installation wizard!
        For any of these questions, simply press Enter to use the default,
        and enter "q" or press Ctrl+C to abort if desired. (Ctrl+D will NOT work)
    """.strip().splitlines()]))
    print()

    cwd = Path(os.getcwd())
    main_dir = cwd / ".raven"

    match ask_option(f"Raven will be installed into '{main_dir}'. Is this okay?",
                    "Yes, continue installation at this directory",
                    "No, use a different directory (must be empty)"):
        case 1: pass
        case 2:
            main_dir = Path(input_tty("Path for installation: ")).expanduser().resolve()
            if main_dir.exists() and not main_dir.is_dir():
                abort("Tried to use file as directory. Aborting...")

    if not main_dir.exists():
        main_dir.mkdir()
    if any(main_dir.iterdir()): # Contains files
        abort("Requires an empty directory. Aborting...")

    source_dir = main_dir / "source"
    build_dir = source_dir / "build"
    bin_dir = main_dir / "bin"

    run(["git", "clone", "https://github.com/late-parrot/craven.git", source_dir], capture_output=True)


    if not build_dir.exists():
        build_dir.mkdir()
    os.chdir(build_dir)
    cmake_flags = skippable_input("CMake flags (see https://github.com/late-parrot/craven for details).")
    run(["cmake", "..", f"-DCMAKE_INSTALL_PREFIX={main_dir}", "-DCMAKE_BUILD_TYPE=Release"] + cmake_flags.split())
    print()


    build_flags = skippable_input("Build flags (see https://github.com/late-parrot/craven for details).")
    run(["cmake", "--build", "."] + build_flags.split())
    print()


    run(["cmake", "--install", "."])
    print()
    os.chdir(cwd)


    if bin_dir not in os.environ.get("PATH", "").split(os.pathsep):
        match ask_option(f"The raven binary is installed in {bin_dir}, which is not on PATH. Would you like to add this directory to PATH?",
                    "Yes, add this directory to PATH using ~/.bashrc or a similar file",
                    "No, I don't want the raven binary on PATH"):
            case 1:
                bashrc = Path(input_tty("Path to .bashrc or similar file: ")).expanduser().resolve()
                if not bashrc.exists() or not bashrc.is_file():
                    abort("\nFile not found. Aborting...")
                is_empty = bashrc.read_text() == ""
                with bashrc.open("a") as f:
                    f.write(("\n" if not is_empty else "") +
                            f"# Add Raven bin directory to PATH\nexport PATH=\"$PATH{os.pathsep}{bin_dir}\"\n")
                print("\nYou will need to either source this file or restart the terminal to see the effect.\n")
            case 2: pass
    

    os.chdir(build_dir)
    match ask_option(f"Would you like to run the test suite to verify your installation? This may take several minutes.",
                    "Yes, run the tests",
                    "No, dont run the tests"):
        case 1:
            run(["ctest"])
            print()
        case 2: pass


    print("\n".join([x.strip() for x in """
        Installation complete!
        Raven is installed and ready to use.
    """.strip().splitlines()]))


if __name__ == "__main__":
    try:
        main()
    except (KeyboardInterrupt, EOFError):
        print("\n")
        abort()
    except Exception as e:
        print(e)
        print()
        abort("Error detected. Aborting...")
