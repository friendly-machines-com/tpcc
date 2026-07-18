program PointerArithmetic;

type
  TWide = record
    First: Integer;
    Second: Integer;
  end;
  PWide = ^TWide;
  PInteger = ^Integer;

var
  Values: array[0..5] of TWide;
  Integers: array[0..5] of Integer;
  FirstWide: PWide;
  LastWide: PWide;
  MiddleWide: PWide;
  FirstInteger: PInteger;
  LastInteger: PInteger;
  Distance: PtrInt;
  RawFirst: Pointer;
  RawLast: Pointer;
  Selected: Integer;

{$ifndef REJECTION_ONLY}
operator UncheckedSubtract(
  First, Second: PInteger): PtrInt;
begin
  Selected := 1;
  if First = Second then
    Result := 100
  else
    Result := 101
end;

operator Subtract(
  First, Second: PInteger): PtrInt;
begin
  Selected := 2;
  if First = Second then
    Result := 200
  else
    Result := 202
end;
{$endif}

begin
  FirstWide := @Values[1];
  LastWide := @Values[5];

  { Typed pointer subtraction is measured in pointed-to elements, not bytes. }
  {$Q-}
  Distance := LastWide - FirstWide;
  if Distance <> 4 then
    Halt(1);
  Distance := FirstWide - LastWide;
  if Distance <> -4 then
    Halt(2);

  { $Q selects the operator identity, but both predefined pointer operations
    retain the same C++ same-array precondition and element result. }
  {$Q+}
  Distance := LastWide - FirstWide;
  if Distance <> 4 then
    Halt(3);

  { Direct pointer stepping uses the same generic result relation as Inc/Dec. }
  MiddleWide := FirstWide + 2;
  if MiddleWide <> @Values[3] then
    Halt(4);
  MiddleWide := LastWide - 2;
  if MiddleWide <> @Values[3] then
    Halt(5);

  {$ifndef REJECTION_ONLY}
  { A complete typed declaration remains an ordinary overload and dominates
    the root fallback. $Q chooses its checked or unchecked identity. }
  FirstInteger := @Integers[0];
  LastInteger := @Integers[5];
  {$Q-}
  Selected := 0;
  if (LastInteger - FirstInteger <> 101) or
     (Selected <> 1) then
    Halt(6);
  {$Q+}
  Selected := 0;
  if (LastInteger - FirstInteger <> 202) or
     (Selected <> 2) then
    Halt(7);
  {$endif}

  {$ifdef REJECT_DIFFERENT_TYPES}
  Distance := FirstWide - FirstInteger;
  {$endif}
  {$ifdef REJECT_UNTYPED_STEP}
  RawFirst := Pointer(@Values[0]);
  Inc(RawFirst);
  {$endif}
  {$ifdef REJECT_UNTYPED_DIFFERENCE}
  RawFirst := Pointer(@Values[0]);
  RawLast := Pointer(@Values[1]);
  Distance := RawLast - RawFirst;
  {$endif}
end.
