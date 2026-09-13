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
function GetExceptionMask: TFPUExceptionMask;
function SetExceptionMask(const Mask: TFPUExceptionMask): TFPUExceptionMask;

implementation

{ These two masks cross the Pascal/C++ boundary as a six-bit byte. Bit 0 is
  exInvalidOp and bit 5 is exPrecision, matching TFPUException's order; the
  RTL keeps the host trap state in sync with it. }
function GetExceptionMaskBits: Byte; external name '::u_math::p_getexceptionmask_bits';
function SetExceptionMaskBits(Bits: Byte): Byte; external name '::u_math::p_setexceptionmask_bits';

function MaskToBits(const Mask: TFPUExceptionMask): Byte;
var
  Bits: Byte;
begin
  Bits := 0;
  if exInvalidOp in Mask then Bits := Bits or $01;
  if exDenormalized in Mask then Bits := Bits or $02;
  if exZeroDivide in Mask then Bits := Bits or $04;
  if exOverflow in Mask then Bits := Bits or $08;
  if exUnderflow in Mask then Bits := Bits or $10;
  if exPrecision in Mask then Bits := Bits or $20;
  Result := Bits
end;

function BitsToMask(Bits: Byte): TFPUExceptionMask;
begin
  Result := [];
  if (Bits and $01) <> 0 then Include(Result, exInvalidOp);
  if (Bits and $02) <> 0 then Include(Result, exDenormalized);
  if (Bits and $04) <> 0 then Include(Result, exZeroDivide);
  if (Bits and $08) <> 0 then Include(Result, exOverflow);
  if (Bits and $10) <> 0 then Include(Result, exUnderflow);
  if (Bits and $20) <> 0 then Include(Result, exPrecision)
end;

function GetExceptionMask: TFPUExceptionMask;
begin
  Result := BitsToMask(GetExceptionMaskBits)
end;

function SetExceptionMask(const Mask: TFPUExceptionMask): TFPUExceptionMask;
begin
  Result := BitsToMask(SetExceptionMaskBits(MaskToBits(Mask)))
end;

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
