program OverflowConstantUnchecked;

{$Q-}
const
  WrappedAdd = High(Integer) + 1;
  WrappedMultiply = High(Integer) * 2;

begin
  if WrappedAdd <> Low(Integer) then
    Halt(1);
  if WrappedMultiply <> -2 then
    Halt(2)
end.
