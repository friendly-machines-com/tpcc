program TypeVarIdentityRejected;

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

{$ifdef TEST_POINTER}
var Actual: TPointerB;
procedure Accept(var Value: TPointerA); begin end;
{$endif}
{$ifdef TEST_STRING}
var Actual: TStringB;
procedure Accept(var Value: TStringA); begin end;
{$endif}
{$ifdef TEST_SET}
var Actual: TSetB;
procedure Accept(var Value: TSetA); begin end;
{$endif}
{$ifdef TEST_RANGE}
var Actual: TRangeB;
procedure Accept(var Value: TRangeA); begin end;
{$endif}
{$ifdef TEST_ARRAY}
var Actual: TArrayB;
procedure Accept(var Value: TArrayA); begin end;
{$endif}
{$ifdef TEST_FILE}
var Actual: TFileB;
procedure Accept(var Value: TFileA); begin end;
{$endif}
{$ifdef TEST_ROUTINE}
var Actual: TRoutineB;
procedure Accept(var Value: TRoutineA); begin end;
{$endif}
{$ifdef TEST_CLASSREF}
var Actual: TClassRefB;
procedure Accept(var Value: TClassRefA); begin end;
{$endif}

begin
  Accept(Actual)
end.
