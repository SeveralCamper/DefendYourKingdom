#include "DYKFramedPanel.h"
#include "Widgets/Layout/SBorder.h"
TSharedRef<SWidget> UDYKFramedPanel::RebuildWidget()
{
	return SNew(SBorder).BorderImage(&MetalRim).Padding(3.f)
	[
		SNew(SBorder).BorderImage(&WoodFrame).Padding(8.f)
		[
			Super::RebuildWidget()
		]
	];
}
