program InvalidImplicitTypeForwards;

{$ifdef TEST_ARRAY_FORWARD}
type
  TContainer = array[0..0] of TLater;
  TLater = record
  end;
{$endif}

{$ifdef TEST_SET_FORWARD}
type
  TContainer = set of TLater;
  TLater = (First, Second);
{$endif}

{$ifdef TEST_FILE_FORWARD}
type
  TContainer = file of TLater;
  TLater = record
  end;
{$endif}

{$ifdef TEST_ALIAS_FORWARD}
type
  TContainer = TLater;
  TLater = Integer;
{$endif}

begin
end.
