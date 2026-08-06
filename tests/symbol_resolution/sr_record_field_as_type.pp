program sr_record_field_as_type;

{ TARGET: RecordType.field used as a type argument. Low(TRec.F) denotes
  the type of field F. }

{$mode delphi}

type
  TRec = record
    F: Integer;
  end;

var
  L: 0..High(TRec.F);

begin
  L := Low(TRec.F);
  if L <> 0 then Halt(1)
end.
