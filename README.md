# Raven

Raven is a dynamically typed, expression-oriented scripting language meant to help
create utilities and act as glue in large projects, or to be used to create full
advanced projects.

## Features

Raven is dynamically typed. This means that the language doesn't really care what
the actual type is, and you can put any value in any variable, create fields
on the fly, and lots more that wouldn't be possible in a static language.

```
var x = 5;       // No type declaration!
x = "a string";  // Different type!
```

Raven is also expression-oriented. This means that *any* code you write can be
considered an expression, which in turn means that instead of code like this

```
if foo == 0 {
    bar(1);
} else {
    bar(foo);
}
```

You can write code like this

```
bar(if foo == 0 { 1 } else { foo });
```

This is an extremely powerful feature, and can be used in a multitude of ways.
For example, most functions never need a single return statement.

```
func increment(x) {
    x + 1
}
```

Notice the lack of a semicolon at the end of the block. This indicates that this
value is the result of the block itself, in turn becoming the return value of
the function.

Finally, Raven is object-oriented. This means that it has a powerful object and
class system, and that most code will seem familiar to almost every developer.

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

## Contribute

If you see a bug or problem in the language, please don't hesitate to submit an issue
here on GitHub.

Contributions to Raven are always welcome! Please fork the project and make your changes,
providing explaination for each change you make. You can then create a pull request, and
it will be reviewed (be patient!). It may be rejected if there is a lack of explaination
for the changes, or if the new features don't align with the rest of the language.

## License

Most of Raven is licensed under Apache 2.0, but since I used Robert Nystrom's book
[Crafting Interpreters](https://craftinginterpreters.com) as a starting point, the
licensing is a bit complicated, with certain parts under MIT. See `LICENSE` for more
detailed explaination.