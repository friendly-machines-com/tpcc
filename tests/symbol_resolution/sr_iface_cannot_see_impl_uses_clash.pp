unit sr_iface_cannot_see_impl_uses_clash;

{ TARGET rule 2f: an identifier available only via the implementation `uses`
  clause is not visible in the interface section. sr_impl_const (impl-uses
  only) exposes `OtherOnly`; referencing it from the interface must fail. }

interface

uses sr_iface_const;            { exposes IfaceConst only }

const
  Bad = OtherOnly;              { TARGET ERROR: OtherOnly not in iface-uses }

implementation

uses sr_impl_const;            { exposes OtherOnly }

end.
