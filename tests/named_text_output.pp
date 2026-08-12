program NamedTextOutput;

var
  F: Text;
  FileName: ShortString;

begin
  FileName := ParamStr(1);
  Assign(F, FileName);

  Rewrite(F);
  Write(F, 'discarded');

  // Rewriting an already-open Text closes it and truncates the same file.
  Rewrite(F);
  Write(F, 'kept');
  Close(F);
  Finalize(F)
end.
