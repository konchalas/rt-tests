<?xml version="1.0"?>
<xsl:stylesheet  version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="text" version="1.0" encoding="UTF-8" indent="no"/>

  <!--                       -->
  <!-- Main report framework -->
  <!--                       -->
  <xsl:template match="/rteval">
    <xsl:text>  ===================================================================&#10;</xsl:text>
    <xsl:text>   rteval (v</xsl:text><xsl:value-of select="@version"/><xsl:text>) report&#10;</xsl:text>
    <xsl:text>  -------------------------------------------------------------------&#10;</xsl:text>
    <xsl:text>   Test run:     </xsl:text>
    <xsl:value-of select="run_info/date"/><xsl:text> </xsl:text><xsl:value-of select="run_info/time"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Run time:     </xsl:text>
    <xsl:value-of select="run_info/@days"/><xsl:text> days </xsl:text>
    <xsl:value-of select="run_info/@hours"/><xsl:text>h </xsl:text>
    <xsl:value-of select="run_info/@minutes"/><xsl:text>m </xsl:text>
    <xsl:value-of select="run_info/@seconds"/><xsl:text>s</xsl:text>
    <xsl:text>&#10;</xsl:text>
    <xsl:if test="run_info/annotate">
      <xsl:text>   Remarks:      </xsl:text>
      <xsl:value-of select="run_info/annotate"/>
    </xsl:if>
    <xsl:text>&#10;&#10;</xsl:text>

    <xsl:text>   Tested node:  </xsl:text>
    <xsl:value-of select="uname/node"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Model:        </xsl:text>
    <xsl:value-of select="HardwareInfo/GeneralInfo/Manufacturer"/>
    <xsl:text> - </xsl:text><xsl:value-of select="HardwareInfo/GeneralInfo/ProductName"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   BIOS version: </xsl:text>
    <xsl:value-of select="HardwareInfo/BIOS"/>
    <xsl:text> (ver: </xsl:text>
    <xsl:value-of select="HardwareInfo/BIOS/@Version"/>
    <xsl:text>, rev :</xsl:text>
    <xsl:value-of select="HardwareInfo/BIOS/@BIOSrevision"/>
    <xsl:text>, release date: </xsl:text>
    <xsl:value-of select="HardwareInfo/BIOS/@ReleaseDate"/>
    <xsl:text>)</xsl:text>
    <xsl:text>&#10;&#10;</xsl:text>

    <xsl:text>   CPU cores:    </xsl:text>
    <xsl:choose>
      <xsl:when test="hardware/cpu_topology">
	<xsl:value-of select="hardware/cpu_topology/@num_cpu_cores"/>
	<xsl:text> (online: </xsl:text>
	<xsl:value-of select="hardware/cpu_topology/@num_cpu_cores_online"/>
	<xsl:text>)</xsl:text>
      </xsl:when>
      <xsl:when test="hardware/cpu_cores">
	<xsl:value-of select="hardware/cpu_cores"/>
      </xsl:when>
      <xsl:otherwise>(unknown)</xsl:otherwise>
    </xsl:choose>
    <xsl:text>&#10;</xsl:text>

    <xsl:if test="hardware/numa_nodes">
      <xsl:text>   NUMA Nodes:   </xsl:text>
      <xsl:value-of select="hardware/numa_nodes"/>
      <xsl:text>&#10;</xsl:text>
    </xsl:if>

    <xsl:text>   Memory:       </xsl:text>
    <xsl:value-of select="hardware/memory_size"/>
    <xsl:choose>
      <xsl:when test="hardware/memory_size/@unit">
	<xsl:value-of select="concat(' ',hardware/memory_size/@unit)"/>
      </xsl:when>
      <xsl:otherwise>
	<xsl:text> kB</xsl:text>
      </xsl:otherwise>
    </xsl:choose>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Kernel:       </xsl:text>
    <xsl:value-of select="uname/kernel"/>
    <xsl:if test="uname/kernel/@is_RT = '1'">  (RT enabled)</xsl:if>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Base OS:      </xsl:text>
    <xsl:value-of select="uname/baseos"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Architecture: </xsl:text>
    <xsl:value-of select="uname/arch"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Clocksource:  </xsl:text>
    <xsl:value-of select="clocksource/current"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>   Available:    </xsl:text>
    <xsl:value-of select="clocksource/available"/>
    <xsl:text>&#10;&#10;</xsl:text>
   
    <xsl:text>   Load commands:&#10;</xsl:text>
    <xsl:text>       Load average: </xsl:text>
    <xsl:value-of select="loads/@load_average"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>       Commands:&#10;</xsl:text>
    <xsl:apply-templates select="loads/command_line"/>
    <xsl:text>&#10;</xsl:text>

   <!-- Format other sections of the report, if they are found                 -->
   <!-- To add support for even more sections, just add them into the existing -->
   <!-- xsl:apply-tempaltes tag, separated with pipe(|)                        -->
   <!--                                                                        -->
   <!--       select="cyclictest|new_foo_section|another_section"              -->
   <!--                                                                        -->
   <xsl:apply-templates select="cyclictest"/>
   <xsl:text>  ===================================================================&#10;</xsl:text>
</xsl:template>
  <!--                              -->
  <!-- End of main report framework -->
  <!--                              -->


  <!--  Formats and lists all used commands lines  -->
  <xsl:template match="command_line">
    <xsl:text>         - </xsl:text>
    <xsl:value-of select="@name"/>
    <xsl:text>: </xsl:text>
    <xsl:choose>
      <xsl:when test="not(@run) or @run = '1'">
	<xsl:value-of select="."/>
      </xsl:when>
      <xsl:otherwise>(Not run)</xsl:otherwise>
    </xsl:choose>
    <xsl:text>&#10;</xsl:text>
  </xsl:template>


  <!-- Format the cyclic test section of the report -->
  <xsl:template match="/rteval/cyclictest">
    <xsl:text>   Latency test&#10;</xsl:text>

    <xsl:text>      Command:</xsl:text>
    <xsl:value-of select="command_line"/>
    <xsl:text>&#10;&#10;</xsl:text>

    <xsl:text>      System:  </xsl:text>
    <xsl:value-of select="system/@description"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>      Statistics: &#10;</xsl:text>
    <xsl:apply-templates select="system/statistics"/>

    <!-- Add CPU core info and stats-->
    <xsl:apply-templates select="core">
      <xsl:sort select="@id" data-type="number"/>
    </xsl:apply-templates>
  </xsl:template>


  <!--  Format the CPU core section in the cyclict test part -->
  <xsl:template match="cyclictest/core">
    <xsl:text>      CPU core </xsl:text>
    <xsl:value-of select="@id"/>
    <xsl:text>   Priority: </xsl:text>
    <xsl:value-of select="@priority"/>
    <xsl:text>&#10;</xsl:text>
    <xsl:text>      Statistics: </xsl:text>
    <xsl:text>&#10;</xsl:text>
    <xsl:apply-templates select="statistics"/>
  </xsl:template>


  <!-- Generic formatting of statistics information -->
  <xsl:template match="statistics">
    <xsl:text>          Samples:           </xsl:text>
    <xsl:value-of select="samples"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Mean:              </xsl:text>
    <xsl:value-of select="mean"/>
    <xsl:value-of select="mean/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Median:            </xsl:text>
    <xsl:value-of select="median"/>
    <xsl:value-of select="median/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Mode:              </xsl:text>
    <xsl:value-of select="mode"/>
    <xsl:value-of select="mode/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Range:             </xsl:text>
    <xsl:value-of select="range"/>
    <xsl:value-of select="range/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Min:               </xsl:text>
    <xsl:value-of select="minimum"/>
    <xsl:value-of select="minimum/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Max:               </xsl:text>
    <xsl:value-of select="maximum"/>
    <xsl:value-of select="maximum/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Mean Absolute Dev: </xsl:text>
    <xsl:value-of select="mean_absolute_deviation"/>
    <xsl:value-of select="mean_absolute_deviation/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Variance:          </xsl:text>
    <xsl:value-of select="variance"/>
    <xsl:value-of select="variance/@unit"/>
    <xsl:text>&#10;</xsl:text>

    <xsl:text>          Std.dev:           </xsl:text>
    <xsl:value-of select="standard_deviation"/>
    <xsl:value-of select="standard_deviation/@unit"/>
    <xsl:text>&#10;&#10;</xsl:text>

  </xsl:template>

</xsl:stylesheet>
