program p;
{$define FVER := 20600}
{$define HAS_FEAT}
var
{$if defined(HAS_FEAT) and (FVER > 20000)}
  a: integer;
{$endif}
{$if defined(HAS_FEAT) or (FVER < 10000)}
  b: integer;
{$endif}
{$if not defined(MISSING)}
  c: integer;
{$endif}
begin
end.
