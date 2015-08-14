<?xml version="1.0"?>
<!--
     *
     *  GPLv2 - Copyright (C) 2009
     *          David Sommerseth <davids@redhat.com>
     *
     *  This program is free software; you can redistribute it and/or
     *  modify it under the terms of the GNU General Public License
     *  as published by the Free Software Foundation; version 2
     *  of the License.
     *
     *  This program is distributed in the hope that it will be useful,
     *  but WITHOUT ANY WARRANTY; without even the implied warranty of
     *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
     *  GNU General Public License for more details.
     *
     *  You should have received a copy of the GNU General Public License
     *  along with this program; if not, write to the Free Software
     *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
     *
-->

<xsl:stylesheet  version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
  <xsl:output method="xml" version="1.0" encoding="UTF-8" indent="yes"/>

  <!-- Used for iterating CPU topology information -->
  <xsl:key name="pkgkey" match="cpu" use="@physical_package_id"/>

  <xsl:template match="/rteval">
    <xsl:choose>
      <!-- TABLE: systems -->
      <xsl:when test="$table = 'systems'">
	<sqldata schemaver="1.0" table="systems" key="syskey">
	  <fields>
            <field fid="0">sysid</field>
            <field fid="1">dmidata</field>
	  </fields>
	  <records>
            <record>
              <value fid="0" hash="sha1">
		<xsl:value-of select="concat(HardwareInfo/@SystemUUID,':',HardwareInfo/@SerialNo)"/>
              </value>
              <value fid="1" type="xmlblob">
		<xsl:copy-of select="HardwareInfo"/>
              </value>
            </record>
	  </records>
	</sqldata>
      </xsl:when>

      <!-- TABLE: systems_hostname -->
      <xsl:when test="$table = 'systems_hostname'">
        <xsl:if test="string(number($syskey)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'syskey' parameter value: </xsl:text><xsl:value-of select="syskey"/>
          </xsl:message>
        </xsl:if>
	<sqldata schemaver="1.0" table="systems_hostname">
	  <fields>
            <field fid="0">syskey</field>
            <field fid="1">hostname</field>
            <field fid="2">ipaddr</field>
	  </fields>
	  <records>
            <record>
              <value fid="0"><xsl:value-of select="$syskey"/></value>
              <value fid="1"><xsl:value-of select="uname/node"/></value>
              <value fid="2"><xsl:value-of select="network_config/interface/IPv4[@defaultgw=1]/@ipaddr"/></value>
            </record>
	  </records>
	</sqldata>
      </xsl:when>

      <!-- TABLE: rtevalruns -->
      <xsl:when test="$table = 'rtevalruns'">
        <xsl:if test="string(number($syskey)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'syskey' parameter value: </xsl:text><xsl:value-of select="syskey"/>
          </xsl:message>
        </xsl:if>
        <xsl:if test="string(number($rterid)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid rterid' parameter value: </xsl:text><xsl:value-of select="$rterid"/>
          </xsl:message>
        </xsl:if>
        <xsl:if test="$report_filename = ''">
          <xsl:message terminate="yes">
            <xsl:text>The parameter 'report_filename' parameter cannot be empty</xsl:text>
          </xsl:message>
        </xsl:if>
	<sqldata schemaver="1.2" table="rtevalruns">
	  <fields>
            <field fid="0">syskey</field>
            <field fid="1">kernel_ver</field>
            <field fid="2">kernel_rt</field>
            <field fid="3">arch</field>
            <field fid="4">run_start</field>
            <field fid="5">run_duration</field>
            <field fid="6">load_avg</field>
            <field fid="7">version</field>
            <field fid="8">report_filename</field>
	    <field fid="9">rterid</field>
	    <field fid="10">submid</field>
	    <field fid="11">distro</field>
	  </fields>
	  <records>
            <record>
              <value fid="0"><xsl:value-of select="$syskey"/></value>
              <value fid="1"><xsl:value-of select="uname/kernel"/></value>
              <value fid="2"><xsl:choose>
		  <xsl:when test="uname/kernel/@is_RT = '1'">true</xsl:when>
		  <xsl:otherwise>false</xsl:otherwise></xsl:choose>
              </value>
              <value fid="3"><xsl:value-of select="uname/arch"/></value>
              <value fid="4"><xsl:value-of select="concat(run_info/date, ' ', run_info/time)"/></value>
              <value fid="5">
		<xsl:value-of select="(run_info/@days*86400)+(run_info/@hours*3600)
                                      +(run_info/@minutes*60)+(run_info/@seconds)"/>
              </value>
              <value fid="6"><xsl:value-of select="loads/@load_average"/></value>
              <value fid="7"><xsl:value-of select="@version"/></value>
              <value fid="8"><xsl:value-of select="$report_filename"/></value>
              <value fid="9"><xsl:value-of select="$rterid"/></value>
              <value fid="10"><xsl:value-of select="$submid"/></value>
	      <value fid="11"><xsl:value-of select="uname/baseos"/></value>
            </record>
	  </records>
	</sqldata>
      </xsl:when>

      <!-- TABLE: rtevalruns_details -->
      <xsl:when test="$table = 'rtevalruns_details'">
        <xsl:if test="string(number($rterid)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'rterid' parameter value: </xsl:text><xsl:value-of select="$rterid"/>
          </xsl:message>
        </xsl:if>
        <sqldata schemaver="1.4" table="rtevalruns_details">
          <fields>
            <field fid="0">rterid</field>
            <field fid="1">numa_nodes</field>
            <field fid="2">num_cpu_cores</field>
            <field fid="3">num_cpu_sockets</field>
            <field fid="4">xmldata</field>
            <field fid="5">annotation</field>
            <field fid="6">cpu_core_spread</field>
          </fields>
          <records>
            <record>
              <value fid="0"><xsl:value-of select="$rterid"/></value>
              <value fid="1"><xsl:value-of select="hardware/numa_nodes"/></value>
              <value fid="2">
                <xsl:choose>
                  <xsl:when test="hardware/cpu_topology">
                    <xsl:value-of select="hardware/cpu_topology/@num_cpu_cores"/>
                  </xsl:when>
                  <xsl:otherwise>
                    <xsl:value-of select="hardware/cpu_cores"/>
                  </xsl:otherwise>
                </xsl:choose>
              </value>
              <value fid="3">
                <xsl:value-of select="hardware/cpu_topology/@num_cpu_sockets"/>
              </value>
              <value fid="4" type="xmlblob">
                <rteval_details>
                  <xsl:copy-of select="clocksource|services|kthreads|network_config|loads|cyclictest/command_line"/>
                  <hardware>
                    <xsl:copy-of select="hardware/memory_size|hardware/cpu_topology"/>
                  </hardware>
                </rteval_details>
              </value>
              <value fid="5"><xsl:value-of select="run_info/annotate"/></value>
              <value fid="6" type="array">
                <xsl:for-each select="hardware/cpu_topology/cpu[generate-id() = generate-id(key('pkgkey', @physical_package_id)[1])]">
                  <xsl:call-template name="count_core_spread">
                    <xsl:with-param name="pkgid" select="@physical_package_id"/>
                  </xsl:call-template>
                </xsl:for-each>
              </value>
            </record>
          </records>
        </sqldata>
      </xsl:when>

      <!-- TABLE: cyclic_statistics -->
      <xsl:when test="$table = 'cyclic_statistics'">
        <xsl:if test="string(number($rterid)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'rterid' parameter value: </xsl:text><xsl:value-of select="$rterid"/>
          </xsl:message>
        </xsl:if>
	<sqldata schemaver="1.1" table="cyclic_statistics">
	  <fields>
            <field fid="0">rterid</field>
            <field fid="1">coreid</field>
            <field fid="2">priority</field>
            <field fid="3">num_samples</field>
            <field fid="4">lat_min</field>
            <field fid="5">lat_max</field>
            <field fid="6">lat_mean</field>
            <field fid="7">mode</field>
            <field fid="8">range</field>
            <field fid="9">median</field>
            <field fid="10">stddev</field>
	    <field fid="11">mean_abs_dev</field>
	    <field fid="12">variance</field>
	  </fields>
	  <records>
            <xsl:for-each select="cyclictest/core/statistics|cyclictest/system/statistics">
              <record>
		<value fid="0"><xsl:value-of select="$rterid"/></value>
		<value fid="1"><xsl:choose>
		    <xsl:when test="../@id"><xsl:value-of select="../@id"/></xsl:when>
		    <xsl:otherwise><xsl:attribute name="isnull">1</xsl:attribute></xsl:otherwise></xsl:choose>
		</value>
		<value fid="2"><xsl:choose>
		    <xsl:when test="../@priority"><xsl:value-of select="../@priority"/></xsl:when>
		    <xsl:otherwise><xsl:attribute name="isnull">1</xsl:attribute></xsl:otherwise></xsl:choose>
		</value>
		<value fid="3"><xsl:value-of select="samples"/></value>
		<value fid="4"><xsl:value-of select="minimum"/></value>
		<value fid="5"><xsl:value-of select="maximum"/></value>
		<value fid="6"><xsl:value-of select="median"/></value>
		<value fid="7"><xsl:value-of select="mode"/></value>
		<value fid="8"><xsl:value-of select="range"/></value>
		<value fid="9"><xsl:value-of select="mean"/></value>
		<value fid="10"><xsl:value-of select="standard_deviation"/></value>
		<value fid="11"><xsl:value-of select="mean_absolute_deviation"/></value>
		<value fid="12"><xsl:value-of select="variance"/></value>
              </record>
            </xsl:for-each>
	  </records>
	</sqldata>
      </xsl:when>

      <!-- TABLE: cyclic_rawdata -->
      <xsl:when test="$table = 'cyclic_rawdata'">
        <xsl:if test="string(number($rterid)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'rterid' parameter value: </xsl:text><xsl:value-of select="$rterid"/>
          </xsl:message>
        </xsl:if>
	<sqldata schemaver="1.0" table="cyclic_rawdata">
	  <fields>
            <field fid="0">rterid</field>
            <field fid="1">cpu_num</field>
            <field fid="2">sampleseq</field>
            <field fid="3">latency</field>
	  </fields>
	  <records>
            <xsl:for-each select="cyclictest/RawSampleData/Thread/Sample">
              <record>
		<value fid="0"><xsl:value-of select="$rterid"/></value>
		<value fid="1"><xsl:value-of select="../@id"/></value>
		<value fid="2"><xsl:value-of select="@seq"/></value>
		<value fid="3"><xsl:value-of select="@latency"/></value>
              </record>
            </xsl:for-each>
	  </records>
	</sqldata>
      </xsl:when>

      <!-- TABLE: cyclic_histogram -->
      <xsl:when test="$table = 'cyclic_histogram'">
        <xsl:if test="string(number($rterid)) = 'NaN'">
          <xsl:message terminate="yes">
            <xsl:text>Invalid 'rterid' parameter value: </xsl:text><xsl:value-of select="$rterid"/>
          </xsl:message>
        </xsl:if>
	<sqldata schemaver="1.0" table="cyclic_histogram">
	  <fields>
            <field fid="0">rterid</field>
            <field fid="1">core</field>
            <field fid="2">index</field>
            <field fid="3">value</field>
	  </fields>
	  <records>
            <xsl:apply-templates select="/rteval/cyclictest/system/histogram/bucket"
				 mode="cyclic_histogram_rec_sql"/>
            <xsl:apply-templates select="/rteval/cyclictest/core/histogram/bucket"
				 mode="cyclic_histogram_rec_sql"/>
	  </records>
	</sqldata>
      </xsl:when>

      <xsl:otherwise>
        <xsl:message terminate="yes">
          <xsl:text>Invalid 'table' parameter value: </xsl:text><xsl:value-of select="$table"/>
        </xsl:message>
      </xsl:otherwise>
    </xsl:choose>
  </xsl:template>

  <xsl:template match="/rteval/cyclictest/system/histogram/bucket|/rteval/cyclictest/core/histogram/bucket"
		mode="cyclic_histogram_rec_sql">
      <record>
	<value fid="0"><xsl:value-of select="$rterid"/></value>
	<value fid="1"><xsl:value-of select="../../@id"/></value>
	<value fid="2"><xsl:value-of select="@index"/></value>
	<value fid="3"><xsl:value-of select="@value"/></value>
      </record>
  </xsl:template>

  <!-- Helper "function" for generating a core per physical socket spread overview -->
  <xsl:template name="count_core_spread">
    <xsl:param name="pkgid"/>
    <value>
      <xsl:value-of select="count(/rteval/hardware/cpu_topology/cpu[@physical_package_id = $pkgid])"/>
    </value>
  </xsl:template>
</xsl:stylesheet>
