unit math;

interface

type
  TValueSign = -1..1;

const
  NegativeValue = Low(TValueSign);
  PositiveValue = High(TValueSign);

function Sign(const AValue: Integer): TValueSign; overload;
function Sign(const AValue: Int64): TValueSign; overload;
function Sign(const AValue: Single): TValueSign; overload;
function Sign(const AValue: Double): TValueSign; overload;
function Sign(const AValue: Extended): TValueSign; overload;

implementation

function Sign(const AValue: Integer): TValueSign;
begin
  if AValue < 0 then
    Result := NegativeValue
  else if AValue > 0 then
    Result := PositiveValue
  else
    Result := 0
end;

function Sign(const AValue: Int64): TValueSign;
begin
  if AValue < 0 then
    Result := NegativeValue
  else if AValue > 0 then
    Result := PositiveValue
  else
    Result := 0
end;

function Sign(const AValue: Single): TValueSign;
begin
  if AValue < 0 then
    Result := NegativeValue
  else if AValue > 0 then
    Result := PositiveValue
  else
    Result := 0
end;

function Sign(const AValue: Double): TValueSign;
begin
  if AValue < 0 then
    Result := NegativeValue
  else if AValue > 0 then
    Result := PositiveValue
  else
    Result := 0
end;

function Sign(const AValue: Extended): TValueSign;
begin
  if AValue < 0 then
    Result := NegativeValue
  else if AValue > 0 then
    Result := PositiveValue
  else
    Result := 0
end;

end.
