program p;
var
{$ifndef HAS_I}
  a: integer;
{$else}
  b: integer;
{$endif}
begin
end.
