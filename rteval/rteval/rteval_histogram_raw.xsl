<?xml version="1.0"?>
<xsl:stylesheet  version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text" version="1.0" encoding="UTF-8" indent="no"/>

  <!--                       -->
  <!-- Main report framework -->
  <!--                       -->
  <xsl:template match="/rteval">
    <!-- Heading -->
    <xsl:text>core&#09;index&#09;value&#10;</xsl:text>

    <!-- Extract overall system histogram data -->
    <xsl:apply-templates select="cyclictest/system/histogram/bucket">
      <xsl:with-param name="label" select="'system'"/> 
      <xsl:sort select="cyclictest/core/histogram/bucket/@index" data-type="number"/>
    </xsl:apply-templates>

    <!-- Extract per cpu core histogram data -->
    <xsl:apply-templates select="cyclictest/core/histogram/bucket">
      <xsl:sort select="cyclictest/core/@id" data-type="number"/>
      <xsl:sort select="cyclictest/core/histogram/bucket/@index" data-type="number"/>
    </xsl:apply-templates>
  </xsl:template>
  <!--                              -->
  <!-- End of main report framework -->
  <!--                              -->

  <!-- Record formatting -->
  <xsl:template match="cyclictest/system/histogram/bucket|cyclictest/core/histogram/bucket">
    <xsl:param name="label"/>
    <xsl:choose>
      <!-- If we don't have a id tag in what should be a 'core' tag, use the given label -->
      <xsl:when test="../../@id"><xsl:value-of select="../../@id"/></xsl:when>
      <xsl:otherwise><xsl:value-of select="$label"/></xsl:otherwise>
    </xsl:choose>
    <xsl:text>&#09;</xsl:text>

    <xsl:value-of select="@index"/>
    <xsl:text>&#09;</xsl:text>

    <xsl:value-of select="@value"/>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>

</xsl:stylesheet>
