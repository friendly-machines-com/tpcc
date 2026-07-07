# What is it

This is a very simple Pascal compiler.  It does all the things in the most straightforward stupid way.

It emits C++20.

It only supports C++ compilers where you can disable strict aliasing.

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
