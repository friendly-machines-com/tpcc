program p;
var
  i: Integer;
  l: LongInt;
procedure foo(x: Integer); overload;
begin
end;
procedure foo(x: LongInt); overload;
begin
end;
begin
  foo(i);
  foo(l)
end.
