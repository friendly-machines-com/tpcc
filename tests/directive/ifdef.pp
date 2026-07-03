program p;
{$define HAS_I}
var
{$ifdef HAS_I}
  i: integer;
{$else}
  j: integer;
{$endif}
begin
end.
