program InvalidRecursiveTypeEquations;

{$ifdef TEST_POINTER_SELF}
type
  TRecursive = ^TRecursive;
{$endif}

{$ifdef TEST_FILE_SELF}
type
  TRecursive = file of TRecursive;
{$endif}

{$ifdef TEST_SET_SELF}
type
  TRecursive = set of TRecursive;
{$endif}

{$ifdef TEST_ROUTINE_SELF}
type
  TRecursive = procedure(Value: TRecursive);
{$endif}

{$ifdef TEST_ARRAY_SELF}
type
  TRecursive = array[0..0] of TRecursive;
{$endif}

begin
end.
