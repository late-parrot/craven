# Raven

Raven is a dynamically-typed, expression-oriented scripting language meant to help
create utilities and act as glue in large projects, or to be used to create full
advanced projects.

## Examples

More in-depth examples and tests can be found in the `test` directory.

```
func hello(name) {
    print "Hello, "+name+"!";
}

hello("world");

func fac(x) {
    if x==1 { 1 } else { x*fac(x-1) }
}

print fac(10);

func foo() {
    var a = 5;
    print a;
    func () {
        a
    }
}
print foo()();
```

As you can see, Raven supports complex features such as closures, and we can see
Raven's expression-orientation at work here -- the last value in a function is
automatically returned, and we use an inline `if` statement in the `fac` function.

## Install

Raven provides a simple installation wizard, which requires Python and CMake to work
(both of these are installed on virtually every system under the sun). Run this command,
preferably at your home directory:

```sh
curl -s https://raw.githubusercontent.com/late-parrot/craven/refs/heads/main/install.py | python
```

On some systems you may need to change `python` to `python3`. Currently there are no
configuration flags that you can pass to CMake or the build command, so simply leave
these blank. The wizard will clone this repository, run CMake and build the project,
then will optionally add the binary to `PATH` and run the unittests.

## Contributing

Contributions to Raven are always welcome! Please fork the project and make your changes,
providing explaination for each change you make. You can then create a pull request, and
it will be reviewed (be patient!). It may be rejected if there is a lack of explaination
for the changes, or if the new features don't align with the rest of the language.
