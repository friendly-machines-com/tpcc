program BlockComments;

var
  Value: Integer;

begin
  Value := 1(* a comment may separate adjacent tokens *)+2;
  (**)
  (* A parenthesized block comment may span
     lines and contain { braces }, // slashes, and isolated * stars. *)
  Value := Value * (2(* comment before a closing parenthesis *));

  (*$define PAREN_COMMENT_DIRECTIVE*)
  {$ifndef PAREN_COMMENT_DIRECTIVE}
  This branch must not be parsed.
  {$endif}

  if Value <> 6 then
    Halt(1)
end.
