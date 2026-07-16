program TypeNarrowingRejected;

type
  TNarrow = 1..10;
  TWide = 1..20;

var
  B: Byte;
  W: Word;
  C: Cardinal;
  I: Integer;
  Narrow: TNarrow;
  Wide: TWide;
  S: Single;
  D: Double;

procedure TakeByte(Value: Byte);
begin
  if Value = 0 then
    B := Value
end;

begin
{$ifdef TEST_INTEGER_ASSIGNMENT}
  B := W;
{$endif}
{$ifdef TEST_SIGNEDNESS_ASSIGNMENT}
  C := I;
{$endif}
{$ifdef TEST_REAL_ASSIGNMENT}
  S := D;
{$endif}
{$ifdef TEST_SUBRANGE_ASSIGNMENT}
  Narrow := Wide;
{$endif}
{$ifdef TEST_BASE_TO_SUBRANGE}
  Narrow := I;
{$endif}
{$ifdef TEST_SINGLETON_ARGUMENT}
  TakeByte(W);
{$endif}
end.
