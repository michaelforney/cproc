# Extensions

In addition to C11, several [GNU extensions] are implemented and more
may be implemented in the future.

## Implemented

### `__asm__` labels

A declarator can be followed by `__asm__("somelabel")` to specify the
assembler name of the object or function. This name is taken literally, so
the resulting symbol will not be mangled according to the target's usual
rules. The name may contain characters not allowed in regular identifiers.

### Built-in functions and types

- **`__builtin_alloca`**: Allocate memory on the stack.
- **`__builtin_constant_p`**: Test whether the argument is a constant expression.
- **`__builtin_inff`**: `float` positive infinity value.
- **`__builtin_nanf`**: `float` quiet NaN value.
- **[`__builtin_offsetof`]**: Return the offset of a member in a struct or union.
- **`__builtin_types_compatible_p`**: Test whether the two types are compatible.
- **`__builtin_va_arg`**: Built-in suitable for implementing the `va_arg` macro.
- **`__builtin_va_copy`**: Built-in suitible for implementing the `va_copy` macro.
- **`__builtin_va_end`**: Built-in suitible for implementing the `va_end` macro.
- **`__builtin_va_list`**: Built-in suitable for implementing the `va_list` type.
- **`__builtin_va_start`**: Built-in suitable for implementing the `va_start` macro.

## Missing

### Statement expressions

In GNU C, you may use a compound statement as expressions when they are
enclosed in parentheses. The last statement in the compound statement
must be an expression statement, whose value is used as the result of
the statement expression.

### Empty declarations

GNU C allows empty top-level declarations (i.e. `;`).

### Missing operand in conditional expression

GNU C allows you to omit the second operand in conditional expressions,
in which case the first operand is used. So `E1 ? : E2` behaves the same
as `E1 ? E1 : E2`, except that `E1` is evaluated only once.

[GNU extensions]: https://gcc.gnu.org/onlinedocs/gcc/C-Extensions.html
[`__builtin_offsetof`]: https://gcc.gnu.org/onlinedocs/gcc/Offsetof.html
