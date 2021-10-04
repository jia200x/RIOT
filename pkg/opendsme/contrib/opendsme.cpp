#include "opendsme/DSMEPlatform.h"
#include "opendsme/opendsme.h"

dsme::DSMEPlatform m_dsme;

int opendsme_init(bool pan_coord)
{
    m_dsme.initialize(pan_coord);
    m_dsme.start();
}

int opendsme_is_associated(void)
{
    return m_dsme.isAssociated();
}
