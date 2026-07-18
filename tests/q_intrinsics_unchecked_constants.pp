program QIntrinsicsUncheckedConstants;

{$Q-}
const
  AbsMinimum = Abs(Low(Integer));
  SuccMaximum = Succ(High(Integer));
  PredMinimum = Pred(Low(Integer));

begin
  if AbsMinimum <> Low(Integer) then Halt(1);
  if SuccMaximum <> Low(Integer) then Halt(2);
  if PredMinimum <> High(Integer) then Halt(3)
end.
