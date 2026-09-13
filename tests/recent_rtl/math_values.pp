program RecentRTLMath;
uses Math;
function TestNaN: Double; external name 'recent_test_nan';
function TestInfinity: Double; external name 'recent_test_infinity';
procedure CheckNear(A, B: Double);
begin
  if Abs(A - B) > 0.000000000001 then Halt(1)
end;
begin
  CheckNear(Pi, 3.141592653589793);
  CheckNear(Sin(0), 0); CheckNear(Cos(0), 1);
  CheckNear(ArcTan(1), Pi / 4);
  CheckNear(Int(-2.75), -2); CheckNear(Frac(-2.75), -0.75);
  if (Sign(LongInt(-4)) <> NegativeValue) or (Sign(Int64(0)) <> 0) or
     (Sign(Single(4)) <> PositiveValue) or (Sign(Double(-4)) <> NegativeValue) or
     (Sign(Extended(4)) <> PositiveValue) then Halt(2);
  if not IsNan(TestNaN) then Halt(3);
  if IsNan(Double(0)) then Halt(4);
  if not IsInfinite(TestInfinity) then Halt(5);
  if IsInfinite(Double(1)) then Halt(6);
  if not IsNan(Single(TestNaN)) or not IsNan(Extended(TestNaN)) then Halt(7);
  if not IsInfinite(Single(TestInfinity)) or not IsInfinite(Extended(TestInfinity)) then Halt(8)
end.
