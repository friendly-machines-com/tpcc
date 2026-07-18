program QDirectiveScopingConstants;

{$push}
{$Q-}
const
  AbsMinimum = Abs {$Q+} (Low(Integer));
{$pop}

{$push}
{$Q-}
const
  SuccMaximum = Succ({$Q+} High(Integer));
{$pop}

{$push}
{$Q-}
const
  PredMinimum = Pred({$Q+} Low(Integer));
{$pop}

begin
  if AbsMinimum <> Low(Integer) then Halt(1);
  if SuccMaximum <> Low(Integer) then Halt(2);
  if PredMinimum <> High(Integer) then Halt(3)
end.
