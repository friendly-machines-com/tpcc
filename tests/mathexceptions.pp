program MathExceptions;
uses Math;
var
  Wanted, Initial, Previous: TFPUExceptionMask;
begin
  { Built with Include rather than a set constant: a folded set constant
    currently drops the enum member's owning unit and emits it unqualified. }
  Wanted := [];
  Include(Wanted, exInvalidOp);
  Include(Wanted, exDenormalized);
  Include(Wanted, exZeroDivide);
  Include(Wanted, exOverflow);
  Include(Wanted, exUnderflow);
  Include(Wanted, exPrecision);

  Initial := GetExceptionMask;
  Previous := SetExceptionMask(Wanted);
  if Previous <> Initial then Halt(1);
  if GetExceptionMask <> Wanted then Halt(2);

  Previous := SetExceptionMask(Initial);
  if Previous <> Wanted then Halt(3);
  if GetExceptionMask <> Initial then Halt(4);

  WriteLn('math exceptions ok')
end.
