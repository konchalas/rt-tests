<?xml version="1.0"?>
<xsl:stylesheet  version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

  <xsl:template match="/dmidecode">
    <HardwareInfo>
      <xsl:attribute name="SerialNo"><xsl:value-of select="SystemInfo/SerialNumber"/></xsl:attribute>
      <xsl:attribute name="SystemUUID"><xsl:value-of select="SystemInfo/SystemUUID"/></xsl:attribute>
      <xsl:copy-of select="/dmidecode/DMIversion"/>
      <xsl:apply-templates select="SystemInfo|BIOSinfo|IPMIdeviceInfo"/>

      <SystemProcessors>
        <xsl:apply-templates select="ProcessorInfo[Populated = 'Enabled']"/>
      </SystemProcessors>

      <SystemCache>
        <xsl:apply-templates select="CacheInfo"/>
      </SystemCache>

      <xsl:apply-templates select="PhysicalMemoryArray"/>

      <SystemDevices>
        <xsl:apply-templates select="SystemSlots"/>
        <xsl:apply-templates select="OnBoardDevicesInfo/dmi_on_board_devices/Device"/>
      </SystemDevices>
      <PortConnectors>
        <xsl:apply-templates select="PortConnectorInfo"/>
      </PortConnectors>
      <xsl:apply-templates select="OEMstrings"/>
    </HardwareInfo>
  </xsl:template>

  <xsl:template match="/dmidecode/BIOSinfo">
    <BIOS>
      <xsl:attribute name="Version"><xsl:value-of select="Version"/></xsl:attribute>
      <xsl:attribute name="ReleaseDate"><xsl:value-of select="ReleaseDate"/></xsl:attribute>
      <xsl:attribute name="BIOSrevision"><xsl:value-of select="BIOSrevision"/></xsl:attribute>
      <xsl:value-of select ="Vendor"/>
    </BIOS>
    <BIOSconfig>
      <xsl:for-each select="Characteristics/flags/flag[@enabled='1']|Characteristics/characteristic[@enabled='1']">
	<characteristic>
	  <xsl:choose>
	    <xsl:when test="local-name(.) = 'flag'">
	      <xsl:attribute name="level"><xsl:value-of select="../../@level"/></xsl:attribute>
	    </xsl:when>
	    <xsl:when test="local-name(.) = 'characteristic'">
	      <xsl:attribute name="level"><xsl:value-of select="../@level"/></xsl:attribute>
	    </xsl:when>
	    <xsl:otherwise/>
	  </xsl:choose>
	  <xsl:value-of select="."/>
	</characteristic>
      </xsl:for-each>
    </BIOSconfig>
  </xsl:template>

  <xsl:template match="/dmidecode/SystemInfo">
    <GeneralInfo>
      <xsl:copy-of select ="Manufacturer|ProductName|Version|SKUnumber|Family"/>
      <BaseBoard>
        <xsl:attribute name="Manufacturer"><xsl:value-of select="../BaseBoardInfo/Manufacturer"/></xsl:attribute>
        <xsl:attribute name="Version"><xsl:value-of select="../BaseBoardInfo/Version"/></xsl:attribute>
        <xsl:attribute name="SerialNum"><xsl:value-of select="../BaseBoardInfo/SerialNumber"/></xsl:attribute>
        <xsl:value-of select="../BaseBoardInfo/ProductName"/>
      </BaseBoard>
      <BaseBoardFeatures>
	<xsl:copy-of select="../BaseBoardInfo/Features/feature"/>
      </BaseBoardFeatures>
      <BootErrors>
        <xsl:value-of select="../SystemBootInfo/Status"/>
      </BootErrors>
    </GeneralInfo>
  </xsl:template>

  <xsl:template match="/dmidecode/IPMIdeviceInfo">
    <IPMInterface>
      <xsl:attribute name="interface"><xsl:value-of select="BaseAddress/@interface"/></xsl:attribute>
      <xsl:value-of select="InterfaceType"/>
    </IPMInterface>
  </xsl:template>

  <xsl:template match="/dmidecode/ProcessorInfo">
    <ProcessorInfo>
      <xsl:attribute name="NumCores"><xsl:value-of select="Cores/CoreCount"/></xsl:attribute>
      <xsl:attribute name="ActiveCores"><xsl:value-of select="Cores/CoresEnabled"/></xsl:attribute>
      <xsl:attribute name="ThreadCount"><xsl:value-of select="Cores/ThreadCount"/></xsl:attribute>
      <xsl:copy-of select="Manufacturer"/>
      <Family>
        <xsl:attribute name="dmiflags"><xsl:value-of select="Family/@flags"/></xsl:attribute>
        <xsl:value-of select="Family"/>
      </Family>
      <Signature><xsl:value-of select="CPUCore/Signature"/></Signature>
      <CPUflags>
        <xsl:for-each select="CPUCore/cpu_flags/flag[@available='1']">
          <Flag>
            <xsl:attribute name="flag"><xsl:value-of select="@flag"/></xsl:attribute>
          </Flag>
        </xsl:for-each>
      </CPUflags>
      <Characterisitics>
        <xsl:for-each select="Cores/Characteristics/Flag">
          <Characteristic><xsl:value-of select="."/></Characteristic>
        </xsl:for-each>
      </Characterisitics>
      <Frequencies>
        <xsl:attribute name="ExternalClock"><xsl:value-of select="Frequencies/ExternalClock"/></xsl:attribute>
        <xsl:attribute name="MaxSpeed"><xsl:value-of select="Frequencies/MaxSpeed"/></xsl:attribute>
        <xsl:attribute name="BootSpeed"><xsl:value-of select="Frequencies/CurrentSpeed"/></xsl:attribute>
      </Frequencies>
      <Cache>
        <xsl:for-each select="Cache/Level[@available='1']">
          <Level>
            <xsl:attribute name="level"><xsl:value-of select="@level"/></xsl:attribute>
            <xsl:attribute name="provided"><xsl:value-of select="@provided"/></xsl:attribute>
          </Level>
        </xsl:for-each>
      </Cache>
    </ProcessorInfo>
  </xsl:template>

  <xsl:template match="/dmidecode/CacheInfo">
    <CacheModule>
      <xsl:attribute name="Loctaion"><xsl:value-of select="CacheLocation"/></xsl:attribute>
      <xsl:attribute name="Size"><xsl:value-of select="concat(InstalledSize, InstalledSize/@unit)"/></xsl:attribute>
      <xsl:attribute name="MaxSize"><xsl:value-of select="concat(MaximumSize, MaximumSize/@unit)"/></xsl:attribute>
      <xsl:copy-of select="OperationalMode|SystemType|Associativity"/>
    </CacheModule>
  </xsl:template>

  <xsl:template match="/dmidecode/PhysicalMemoryArray">
    <SystemMemory>
      <xsl:attribute name="MaxCapacity"><xsl:value-of select="MaxCapacity"/></xsl:attribute>
      <xsl:attribute name="MaxCapacityUnit"><xsl:value-of select="MaxCapacity/@unit"/></xsl:attribute>
      <xsl:attribute name="MaxNumSlots"><xsl:value-of select="@NumDevices"/></xsl:attribute>
      <xsl:apply-templates select="../MemoryDevice" mode="module"/>
    </SystemMemory>
  </xsl:template>

  <xsl:template match="/dmidecode/MemoryDevice" mode="module">
    <xsl:if test="not(Size/@empty)">
      <MemoryModule>
        <xsl:attribute name="Slot"><xsl:value-of select="Locator"/></xsl:attribute>
        <xsl:attribute name="BankLocator"><xsl:value-of select="BankLocator"/></xsl:attribute>
        <xsl:attribute name="Set"><xsl:value-of select="Set"/></xsl:attribute>
        <xsl:copy-of select="Size"/>
        <xsl:copy-of select="Speed"/>
        <xsl:copy-of select="Type"/>
        <Manufacturer>
          <xsl:attribute name="PartNumber"><xsl:value-of select="PartNumber"/></xsl:attribute>
          <xsl:attribute name="AssetTag"><xsl:value-of select="AssetTag"/></xsl:attribute>
          <xsl:value-of select="Manufacturer"/>
        </Manufacturer>
      </MemoryModule>
    </xsl:if>
  </xsl:template>

  <xsl:template match="/dmidecode/SystemSlots">
    <SystemSlot>
      <xsl:attribute name="id"><xsl:value-of select="SlotID/@id"/></xsl:attribute>
      <xsl:attribute name="SlotDesignation"><xsl:value-of select="Designation"/></xsl:attribute>
      <xsl:attribute name="Width"><xsl:value-of select="SlotWidth"/></xsl:attribute>
      <xsl:attribute name="Type"><xsl:value-of select="SlotType"/></xsl:attribute>
      <xsl:attribute name="Usage"><xsl:value-of select="CurrentUsage"/></xsl:attribute>
    </SystemSlot>
  </xsl:template>

  <xsl:template match="/dmidecode/OnBoardDevicesInfo/dmi_on_board_devices/Device">
    <OnBoardDevice>
      <xsl:attribute name="Enabled"><xsl:value-of select="@Enabled"/></xsl:attribute>
      <xsl:attribute name="Type"><xsl:value-of select="Type"/></xsl:attribute>
      <xsl:value-of select="Description"/>
    </OnBoardDevice>
  </xsl:template>

  <xsl:template match="/dmidecode/PortConnectorInfo">
    <Connector>
      <xsl:attribute name="DesignatorInt"><xsl:value-of select="DesignatorRef[@type='internal']"/></xsl:attribute>
      <xsl:attribute name="DesignatorExt"><xsl:value-of select="DesignatorRef[@type='external']"/></xsl:attribute>
      <xsl:attribute name="Connector"><xsl:value-of select="Connector[@type='external']"/></xsl:attribute>
      <xsl:value-of select="PortType"/>
    </Connector>
  </xsl:template>

  <xsl:template match="/dmidecode/OEMstrings">
    <OEMstrings>
      <xsl:for-each select="Record">
	<OEMstring>
	  <xsl:attribute name="index"><xsl:value-of select="@index"/></xsl:attribute>
	  <xsl:value-of select="."/>
	</OEMstring>
      </xsl:for-each>
    </OEMstrings>
  </xsl:template>

</xsl:stylesheet>
