# What is it

This is a very simple Pascal compiler.  It does all the things in the most straightforward stupid way.

It emits C++20.

It only supports C++ compilers where you can disable strict aliasing.

# Target ABI assumptions

TPCC currently assumes a GNOME/GObject-capable data-pointer callback ABI:
ordinary object and data pointer types have the same representation and are
passed in the same argument locations.

Pascal explicit casts between two plain routine types, or between two
`procedure of object` types, may therefore change a by-value data-pointer
parameter type while retaining the routine's code pointer.  This is the same
ABI convention used by `G_CALLBACK`: GObject invokes the erased callback using
the signal's registered pointer parameter types and cannot recover the
callback's original C declaration.

Calling a function through the resulting incompatible C++ function-pointer
type is not defined by portable ISO C++20.  TPCC deliberately accepts that
ABI-level operation only for explicit Pascal casts and only for by-value data
pointer parameters.  Results, arity, parameter modes, non-pointer parameters,
and the distinction between a one-word plain routine and a two-word bound
method remain exact.

# Limitations

Not supported:

```pascal
type
  TFoo = class;

type
  TFooClass = class of TFoo;

// later:
type
  TFoo = class(TObject)
  end;
```
Supported:

```pascal
type
  TFoo = class;
  TFooClass = class of TFoo;
  TFoo = class(TObject)
  end;
```

Supported:

```pascal
type
  TFoo = class(TObject)
  end;
  TFooClass = class of TFoo;
```
