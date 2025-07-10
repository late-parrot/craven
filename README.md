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

## Building and install

For now, Raven does not have a full installer or build system. Instead, you can
clone the repository, and then build it using the following commands:

```
gcc $(find src/*.c) -lm -o build/craven.out
```

This will build the project and place the executable at `build/craven.out`. You can
now run Raven with:

```
./build/craven.out [FILE]
```

Providing a file is optional, and if no file is provided, a REPL session will be
started.

## Contributing

Contributions to Raven are always welcome! Please fork the project and make your changes,
providing explaination for each change you make. You can then create a pull request, and
it will be reviewed (be patient!). It may be rejected if there is a lack of explaination
for the changes, or if the new features don't align with the rest of the language.