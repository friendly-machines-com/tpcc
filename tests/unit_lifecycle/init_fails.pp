unit Init_Fails;

interface

uses Init_Leaf;

implementation

initialization
  Write('X');
  Halt(7)
finalization
  Write('x')
end.
