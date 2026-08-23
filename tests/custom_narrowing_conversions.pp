program CustomNarrowingConversions;

type
  TSource = record
    Value: Integer;
  end;
  TNarrow = record
    Value: Integer;
  end;
  TRegular = record
    Value: Integer;
  end;

var
  Source: TSource;
  Narrow: TNarrow;
  Selected: Integer;

{$ifndef OMIT_UNCHECKED}
operator UncheckedImplicitNarrowing(
  const Value: TSource): TNarrow;
begin
  Selected := Selected + 1;
  Result.Value := Value.Value + 100
end;
{$endif}

{$ifndef OMIT_CHECKED}
operator ImplicitNarrowing(
  const Value: TSource): TNarrow;
begin
  Selected := Selected + 10;
  Result.Value := Value.Value + 200
end;
{$endif}

operator UncheckedImplicit(
  const Value: TSource): TRegular;
begin
  Selected := Selected + 100;
  Result.Value := Value.Value + 300
end;

operator Implicit(
  const Value: TSource): TRegular;
begin
  Selected := Selected + 1000;
  Result.Value := Value.Value + 400
end;

operator +(
  const Left, Right: TNarrow): TNarrow;
begin
  Result.Value := Left.Value + Right.Value
end;

function TakeNarrow(Value: TNarrow): Integer;
begin
  Result := Value.Value
end;

function PreferRegular(Value: TNarrow): Integer; overload;
begin
  if Value.Value = Value.Value then
    Result := 1
end;

function PreferRegular(Value: TRegular): Integer; overload;
begin
  if Value.Value = Value.Value then
    Result := 2
end;

function PreferExact(Value: TNarrow): Integer; overload;
begin
  if Value.Value = Value.Value then
    Result := 3
end;

function PreferExact(Value: TSource): Integer; overload;
begin
  if Value.Value = Value.Value then
    Result := 4
end;

{ Ordinary routines with these source names are not conversion declarations.
  They must retain ordinary call syntax and overload identity. }
function ImplicitNarrowing(Value: Integer): Integer;
begin
  Result := Value + 1000
end;

function UncheckedImplicitNarrowing(Value: Integer): Integer;
begin
  Result := Value + 2000
end;

begin
  Source.Value := 1;

  {$R-}
  Selected := 0;
  Narrow := Source;
  if (Selected <> 1) or (Narrow.Value <> 101) then
    Halt(1);

  Selected := 0;
  if TakeNarrow(Source) <> 101 then
    Halt(2);
  if Selected <> 1 then
    Halt(3);

  Selected := 0;
  Narrow := Source + Source;
  if (Selected <> 2) or (Narrow.Value <> 202) then
    Halt(4);

  Selected := 0;
  if PreferRegular(Source) <> 2 then
    Halt(5);
  if Selected <> 100 then
    Halt(6);

  {$R+}
  Selected := 0;
  Narrow := Source;
  if (Selected <> 10) or (Narrow.Value <> 201) then
    Halt(7);

  Selected := 0;
  if TakeNarrow(Source) <> 201 then
    Halt(8);
  if Selected <> 10 then
    Halt(9);

  Selected := 0;
  Narrow := Source + Source;
  if (Selected <> 20) or (Narrow.Value <> 402) then
    Halt(10);

  Selected := 0;
  if PreferRegular(Source) <> 2 then
    Halt(11);
  if Selected <> 1000 then
    Halt(12);

  Selected := 0;
  if PreferExact(Source) <> 4 then
    Halt(13);
  if Selected <> 0 then
    Halt(14);

  if ImplicitNarrowing(3) <> 1003 then
    Halt(15);
  if UncheckedImplicitNarrowing(4) <> 2004 then
    Halt(16)
end.
