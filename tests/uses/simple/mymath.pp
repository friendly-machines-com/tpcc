unit mymath;
interface
procedure clear(var x: integer);
procedure set_to_one(var y: integer);
implementation
procedure clear;
begin
  x := 0
end;
procedure set_to_one(var z: integer);
begin
  z := 1
end;
end.
