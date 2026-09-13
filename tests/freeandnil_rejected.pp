program FreeAndNilRejected;
uses SysUtils;
type
  TChild = class
  public
    Value: Integer;
    property Item: Integer read Value write Value;
  end;
  TRecord = record Value: Integer end;
var
  I: Integer;
  P: Pointer;
  R: TRecord;
  Meta: TClass;
  Obj: TChild;
procedure OrdinaryVar(var Value: TObject);
begin end;
begin
{$ifdef INTEGER_ARG} FreeAndNil(I); {$endif}
{$ifdef POINTER_ARG} FreeAndNil(P); {$endif}
{$ifdef RECORD_ARG} FreeAndNil(R); {$endif}
{$ifdef METACLASS_ARG} FreeAndNil(Meta); {$endif}
{$ifdef NIL_ARG} FreeAndNil(nil); {$endif}
{$ifdef TEMPORARY_ARG} FreeAndNil(TChild.Create); {$endif}
{$ifdef PROPERTY_ARG} FreeAndNil(Obj.Item); {$endif}
{$ifdef ORDINARY_VAR} OrdinaryVar(Obj); {$endif}
end.
