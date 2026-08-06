program sr_object_field_as_type;

{ TARGET: ObjectType.field used as a type argument. Same semantics as
  RecordType.field. }

{$mode delphi}

type
  PObj = ^TObj;
  TObj = object
    F: Integer;
  end;

var
  L: 0..High(TObj.F);

begin
  L := Low(TObj.F);
  if L <> 0 then Halt(1)
end.
