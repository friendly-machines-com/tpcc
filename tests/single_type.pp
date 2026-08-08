program single_type;

const
  FoldedCast: Single = Single(1.25);
  FoldedCoerce: Single = Single(2.5);

type
  TBase = class
    destructor Destroy; virtual;
  end;

type
  TChild = class(TBase)
  end;

var
  S: Single;
  D: Double;
  E: Extended;
  Casted: Single;
  Coerced: Single;
  Sum: Single;
  Product: Single;
  Quotient: Single;
  Parsed: Single;
  ParseCode: LongInt;
  IntegerValue: Integer;
  RankSingle: LongInt;
  RankDouble: LongInt;
  RankExtended: LongInt;
  RankInteger: LongInt;
  RankTypedInteger: LongInt;
  RankLiteral: LongInt;
  SingleLess: Boolean;
  SingleSize: SizeInt;
  Base: TBase;
  Child: TChild;
  IsChild: Boolean;

destructor TBase.Destroy;
begin
end;

function Rank(Value: Single): LongInt; overload;
begin
  if Value = Value then
    Rank := 1
  else
    Rank := 0
end;

function Rank(Value: Double): LongInt; overload;
begin
  if Value = Value then
    Rank := 2
  else
    Rank := 0
end;

function Rank(Value: Extended): LongInt; overload;
begin
  if Value = Value then
    Rank := 3
  else
    Rank := 0
end;

begin
  S := FoldedCast + FoldedCoerce;
  D := S;
  E := D;
  Casted := Single(D);
  Coerced := Single(E);
  Sum := S + Single(0.25);
  Product := S * Single(2.0);
  Quotient := Product / Single(2.0);
  SingleLess := Single(1.0) < Single(2.0);
  SingleSize := SizeOf(Single);

  RankSingle := Rank(S);
  RankDouble := Rank(D);
  RankExtended := Rank(E);
  RankInteger := Rank(1);
  IntegerValue := 1;
  RankTypedInteger := Rank(IntegerValue);
  RankLiteral := Rank(1.0);

  Val('2.25', Parsed, ParseCode);

  Child := Base as TChild;
  IsChild := Base is TChild
end.
