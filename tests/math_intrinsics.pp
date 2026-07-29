program MathIntrinsics;

const
  CTrunc = Trunc(7 / 2);
  CRoundEven = Round(5 / 2);
  CFrac = Frac(7 / 2);
  CIntegerSqr = Sqr(Integer(12));
  CRealSqr = Sqr(1.5);
  CSqrt = Sqrt(81);
  CExp = Exp(0);
  CLn = Ln(1);

var
  D: Double;
  E: Extended;
  Ten: Extended;
  A, B: LongInt;
  N: LongInt;
  SqrCalls: Integer;

function SqrArgument: Integer;
begin
  SqrCalls := SqrCalls + 1;
  Result := 3
end;

begin
  if CTrunc <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  if CRoundEven <> 2 then
    begin
      N := 0;
      N := 1 div N
    end;
  Ten := 10;
  if Trunc(CFrac * Ten) <> 5 then
    begin
      N := 0;
      N := 1 div N
    end;
  if CIntegerSqr <> 144 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Trunc(CRealSqr * 4) <> 9 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Trunc(CSqrt) <> 9 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Trunc(CExp) <> 1 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Trunc(CLn) <> 0 then
    begin
      N := 0;
      N := 1 div N
    end;

  D := 1.5;
  E := Sqr(D);
  if Trunc(E * 4) <> 9 then
    begin
      N := 0;
      N := 1 div N
    end;

  A := 7;
  B := 2;
  D := 7 / 2;
  E := D;
  if Trunc(E) <> 3 then
    begin
      N := 0;
      N := 1 div N
    end;
  D := E;
  if Round(D) <> 4 then
    begin
      N := 0;
      N := 1 div N
    end;
  if Trunc(Frac(-(A / B)) * Ten) <> -5 then
    begin
      N := 0;
      N := 1 div N
    end;

  { FPC's Integer Sqr is unchecked even when call-site overflow checking is
    enabled. TPCC must wrap without invoking signed C++ overflow. }
{$Q+}
  N := 50000;
  N := Sqr(N);
{$Q-}
  if N <> -1794967296 then
    begin
      N := 0;
      N := 1 div N
    end;

  SqrCalls := 0;
  if Sqr(SqrArgument()) <> 9 then
    begin
      N := 0;
      N := 1 div N
    end;
  if SqrCalls <> 1 then
    begin
      N := 0;
      N := 1 div N
    end
end.
