program sr_record_no_such_field;

{ TARGET: RecordType.NoSuchField as a type argument--RHS not in the
  record's frame. }

{$mode delphi}

type
  TRec = record
    F: Integer;
  end;

var
  L: 0..High(TRec.NoSuchField);

begin
end.
