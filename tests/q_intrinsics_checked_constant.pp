program QIntrinsicsCheckedConstant;

{$Q+}
const
{$ifdef TEST_ABS}
  Overflow = Abs(Low(Integer));
{$endif}
{$ifdef TEST_SUCC}
  Overflow = Succ(High(Integer));
{$endif}
{$ifdef TEST_PRED}
  Overflow = Pred(Low(Integer));
{$endif}

begin
end.
