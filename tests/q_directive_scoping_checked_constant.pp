program QDirectiveScopingCheckedConstant;

{$push}
{$Q+}
const
{$ifdef TEST_ABS}
  Overflow = Abs({$Q-} Low(Integer));
{$endif}
{$ifdef TEST_SUCC}
  Overflow = Succ({$Q-} High(Integer));
{$endif}
{$ifdef TEST_PRED}
  Overflow = Pred({$Q-} Low(Integer));
{$endif}
{$pop}

begin
end.
