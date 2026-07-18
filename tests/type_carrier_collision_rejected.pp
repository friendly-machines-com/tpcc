program TypeCarrierCollisionRejected;

type
  TPointerA = ^Integer;
  TPointerB = ^Integer;
  TStringA = string[12];
  TStringB = string[12];
  TSetA = set of Byte;
  TSetB = set of Byte;
  TRangeA = 1..5;
  TRangeB = 1..5;
  TArrayA = array[0..1] of Integer;
  TArrayB = array[0..1] of Integer;
  TFileA = file of Integer;
  TFileB = file of Integer;
  TRoutineA = procedure(Value: Integer);
  TRoutineB = procedure(Value: Integer);
  TObjectClass = class
  end;
  TClassRefA = class of TObjectClass;
  TClassRefB = class of TObjectClass;
  TConversionSource = record
    Value: Integer;
  end;

{$ifdef TEST_POINTER}
function Clash(Value: TPointerA): Integer; overload; forward;
function Clash(Value: TPointerB): Integer; overload; forward;
{$endif}
{$ifdef TEST_STRING}
function Clash(Value: TStringA): Integer; overload; forward;
function Clash(Value: TStringB): Integer; overload; forward;
{$endif}
{$ifdef TEST_SET}
function Clash(Value: TSetA): Integer; overload; forward;
function Clash(Value: TSetB): Integer; overload; forward;
{$endif}
{$ifdef TEST_RANGE}
function Clash(Value: TRangeA): Integer; overload; forward;
function Clash(Value: TRangeB): Integer; overload; forward;
{$endif}
{$ifdef TEST_ARRAY}
function Clash(Value: TArrayA): Integer; overload; forward;
function Clash(Value: TArrayB): Integer; overload; forward;
{$endif}
{$ifdef TEST_FILE}
function Clash(var Value: TFileA): Integer; overload; forward;
function Clash(var Value: TFileB): Integer; overload; forward;
{$endif}
{$ifdef TEST_ROUTINE}
function Clash(Value: TRoutineA): Integer; overload; forward;
function Clash(Value: TRoutineB): Integer; overload; forward;
{$endif}
{$ifdef TEST_CLASSREF}
function Clash(Value: TClassRefA): Integer; overload; forward;
function Clash(Value: TClassRefB): Integer; overload; forward;
{$endif}
{$ifdef TEST_CONVERSION}
operator :=(const Source: TConversionSource): TStringA; forward;
operator implicit(const Source: TConversionSource): TStringB; forward;
{$endif}
{$ifdef TEST_EXPLICIT_CONVERSION}
operator explicit(const Source: TConversionSource): TStringA; forward;
operator explicit(const Source: TConversionSource): TStringB; forward;
{$endif}

begin
end.
