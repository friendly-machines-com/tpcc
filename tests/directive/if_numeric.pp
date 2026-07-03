program p;
{$define FVER := 20600}
var
{$if FVER < 20700}
  older: integer;
{$endif}
{$if FVER = 20600}
  exact: integer;
{$endif}
{$if FVER > 20000}
  newer: integer;
{$endif}
begin
end.
