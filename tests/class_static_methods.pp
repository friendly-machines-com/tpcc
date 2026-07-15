program class_static_methods;

type
  TPlainFunction = function(Value: Integer): Integer;
  TBoundFunction = function(Value: Integer): Integer of object;

  TBase = class
  public
    class var Count: Integer;
    class function NormalValue(Value: Integer): Integer;
    class function NormalClassName: ShortString;
    class function StaticValue(Value: Integer): Integer; static;
    class function StaticCallsNormal: ShortString; static;
    class function StaticTag: Integer; static;
  end;

  TChild = class(TBase)
  public
    class function InheritedStaticTag: Integer; static;
  end;

  TBaseClass = class of TBase;

  TRecord = record
    class function Twice(Value: Integer): Integer; static;
  end;

  TPackedRecord = packed record
    class function Thrice(Value: Integer): Integer; static;
  end;

class function TBase.NormalValue(Value: Integer): Integer;
begin
  Result := Value + 10
end;

class function TBase.NormalClassName: ShortString;
begin
  Result := Self.ClassName
end;

class function TBase.StaticValue(Value: Integer): Integer;
begin
  Count := Count + 1;
  Result := Value * 2
end;

class function TBase.StaticCallsNormal: ShortString;
begin
  Result := NormalClassName
end;

class function TBase.StaticTag: Integer;
begin
  Result := 40
end;

class function TChild.InheritedStaticTag: Integer;
begin
  Result := inherited StaticTag + 2
end;

class function TRecord.Twice(Value: Integer): Integer;
begin
  Result := Value * 2
end;

class function TPackedRecord.Thrice(Value: Integer): Integer;
begin
  Result := Value * 3
end;

var
  QualifierEvaluations: Integer;

function GetObject: TBase;
begin
  QualifierEvaluations := QualifierEvaluations + 1;
  Result := nil
end;

function GetClass: TBaseClass;
begin
  QualifierEvaluations := QualifierEvaluations + 1;
  Result := nil
end;

var
  PlainFunction: TPlainFunction;
  BoundFunction: TBoundFunction;
begin
  if TChild.StaticValue(3) <> 6 then
    Halt(1);
  if TChild.NormalClassName <> 't_tchild' then
    Halt(2);
  if TChild.StaticCallsNormal <> 't_tbase' then
    Halt(3);
  if TChild.InheritedStaticTag <> 42 then
    Halt(4);
  if TRecord.Twice(5) <> 10 then
    Halt(5);
  if TPackedRecord.Thrice(5) <> 15 then
    Halt(6);

  if GetObject.StaticValue(6) <> 12 then
    Halt(7);
  if GetClass.StaticValue(7) <> 14 then
    Halt(8);
  if QualifierEvaluations <> 2 then
    Halt(9);

  PlainFunction := @GetObject.StaticValue;
  if PlainFunction(8) <> 16 then
    Halt(10);
  if QualifierEvaluations <> 3 then
    Halt(11);
  BoundFunction := @TBase.NormalValue;
  if BoundFunction(9) <> 19 then
    Halt(12);

  if TBase.Count <> 4 then
    Halt(13);
  WriteLn('class static methods passed')
end.
