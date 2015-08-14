-- Create rteval database users
--
CREATE USER rtevxmlrpc NOSUPERUSER ENCRYPTED PASSWORD 'rtevaldb';
CREATE USER rtevparser NOSUPERUSER ENCRYPTED PASSWORD 'rtevaldb_parser';

-- Create rteval database
--
CREATE DATABASE rteval ENCODING 'utf-8';

\c rteval

-- TABLE: rteval_info
-- Contains information the current rteval XML-RPC and parser installation
--
    CREATE TABLE rteval_info (
       key    varchar(32) NOT NULL,
       value  TEXT NOT NULL,
       rtiid  SERIAL,
       PRIMARY KEY(rtiid)
    );
    GRANT SELECT ON rteval_info TO rtevparser;
    INSERT INTO rteval_info (key, value) VALUES ('sql_schema_ver','1.3');

-- Enable plpgsql.  It is expected that this PL/pgSQL is available.
    CREATE LANGUAGE 'plpgsql';

-- FUNCTION: trgfnc_submqueue_notify
-- Trigger function which is called on INSERT queries to the submissionqueue table.
-- It will send a NOTIFY rteval_submq on INSERTs.
--
    CREATE FUNCTION trgfnc_submqueue_notify() RETURNS TRIGGER
    AS $BODY$
      DECLARE
      BEGIN
        NOTIFY rteval_submq;
        RETURN NEW;
      END
    $BODY$ LANGUAGE 'plpgsql';

    -- The user(s) which are allowed to do INSERT on the submissionqueue
    -- must also be allowed to call this trigger function.
    GRANT EXECUTE ON FUNCTION trgfnc_submqueue_notify() TO rtevxmlrpc;

-- TABLE: submissionqueue
-- All XML-RPC clients registers their submissions into this table.  Another parser thread
-- will pickup the records where parsestart IS NULL.
--
    CREATE TABLE submissionqueue (
           clientid   varchar(128) NOT NULL,
           filename   VARCHAR(1024) NOT NULL,
           status     INTEGER DEFAULT '0',
           received   TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT NOW(),
           parsestart TIMESTAMP WITH TIME ZONE,
           parseend   TIMESTAMP WITH TIME ZONE,
           submid     SERIAL,
           PRIMARY KEY(submid)
    ) WITH OIDS;
    CREATE INDEX submissionq_status ON submissionqueue(status);

    CREATE TRIGGER trg_submissionqueue AFTER INSERT
           ON submissionqueue FOR EACH STATEMENT
	   EXECUTE PROCEDURE trgfnc_submqueue_notify();

    GRANT SELECT, INSERT ON submissionqueue TO rtevxmlrpc;
    GRANT USAGE ON submissionqueue_submid_seq TO rtevxmlrpc;
    GRANT SELECT, UPDATE ON submissionqueue TO rtevparser;

-- TABLE: systems
-- Overview table over all systems which have sent reports
-- The dmidata column will keep the complete DMIdata available
-- for further information about the system.
--
    CREATE TABLE systems (
        syskey        SERIAL NOT NULL,
        sysid         VARCHAR(64) NOT NULL,
        dmidata       xml NOT NULL,
        PRIMARY KEY(syskey)
    ) WITH OIDS;

    GRANT SELECT,INSERT ON systems TO rtevparser;
    GRANT USAGE ON systems_syskey_seq TO rtevparser;

-- TABLE: systems_hostname
-- This table is used to track the hostnames and IP addresses
-- a registered system have used over time
--
   CREATE TABLE systems_hostname (
        syskey        INTEGER REFERENCES systems(syskey) NOT NULL,
        hostname      VARCHAR(256) NOT NULL,
        ipaddr        cidr
    ) WITH OIDS;
    CREATE INDEX systems_hostname_syskey ON systems_hostname(syskey);
    CREATE INDEX systems_hostname_hostname ON systems_hostname(hostname);
    CREATE INDEX systems_hostname_ipaddr ON systems_hostname(ipaddr);

    GRANT SELECT, INSERT ON systems_hostname TO rtevparser;


-- TABLE: rtevalruns
-- Overview over all rteval runs, when they were run and how long they ran.
--
    CREATE TABLE rtevalruns (
        rterid          SERIAL NOT NULL, -- RTEval Run Id
        submid          INTEGER REFERENCES submissionqueue(submid) NOT NULL,
        syskey          INTEGER REFERENCES systems(syskey) NOT NULL,
        kernel_ver      VARCHAR(32) NOT NULL,
        kernel_rt       BOOLEAN NOT NULL,
        arch            VARCHAR(12) NOT NULL,
	distro		VARCHAR(64),
        run_start       TIMESTAMP WITH TIME ZONE NOT NULL,
        run_duration    INTEGER NOT NULL,
        load_avg        REAL NOT NULL,
        version         VARCHAR(4), -- Version of rteval
        report_filename TEXT,
        PRIMARY KEY(rterid)
    ) WITH OIDS;

    GRANT SELECT,INSERT ON rtevalruns TO rtevparser;
    GRANT USAGE ON rtevalruns_rterid_seq TO rtevparser;

-- TABLE rtevalruns_details
-- More specific information on the rteval run.  The data is stored
-- in XML for flexibility
--
-- Tags being saved here includes: /rteval/clocksource, /rteval/hardware,
-- /rteval/loads and /rteval/cyclictest/command_line
--
    CREATE TABLE rtevalruns_details (
        rterid          INTEGER REFERENCES rtevalruns(rterid) NOT NULL,
        annotation      TEXT,
        num_cpu_cores   INTEGER,
        num_cpu_sockets INTEGER,
        numa_nodes      INTEGER,
        xmldata         xml NOT NULL,
        PRIMARY KEY(rterid)
    );
    GRANT INSERT ON rtevalruns_details TO rtevparser;

-- TABLE: cyclic_statistics
-- This table keeps statistics overview over a particular rteval run
--
    CREATE TABLE cyclic_statistics (
        rterid        INTEGER REFERENCES rtevalruns(rterid) NOT NULL,
        coreid        INTEGER, -- NULL=system
        priority      INTEGER, -- NULL=system
        num_samples   BIGINT NOT NULL,
        lat_min       REAL NOT NULL,
        lat_max       REAL NOT NULL,
        lat_mean      REAL NOT NULL,
        mode          REAL NOT NULL,
        range         REAL NOT NULL,
        median        REAL NOT NULL,
        stddev        REAL NOT NULL,
	mean_abs_dev  REAL NOT NULL,
	variance      REAL NOT NULL,
        cstid         SERIAL NOT NULL, -- unique record ID
        PRIMARY KEY(cstid)
    ) WITH OIDS;
    CREATE INDEX cyclic_statistics_rterid ON cyclic_statistics(rterid);

    GRANT INSERT ON cyclic_statistics TO rtevparser;
    GRANT USAGE ON cyclic_statistics_cstid_seq TO rtevparser;

-- TABLE: cyclic_histogram
-- This table keeps the raw histogram data for each rteval run being
-- reported.
--
    CREATE TABLE cyclic_histogram (
        rterid        INTEGER REFERENCES rtevalruns(rterid) NOT NULL,
        core          INTEGER, -- NULL=system
        index         INTEGER NOT NULL,
        value         BIGINT NOT NULL
    ) WITHOUT OIDS;
    CREATE INDEX cyclic_histogram_rterid ON cyclic_histogram(rterid);

    GRANT INSERT ON cyclic_histogram TO rtevparser;

-- TABLE: cyclic_rawdata
-- This table keeps the raw data for each rteval run being reported.
-- Due to that it will be an enormous amount of data, we avoid using
-- OID on this table.
--
    CREATE TABLE cyclic_rawdata (
        rterid        INTEGER REFERENCES rtevalruns(rterid) NOT NULL,
        cpu_num       INTEGER NOT NULL,
        sampleseq     INTEGER NOT NULL,
        latency       REAL NOT NULL
    ) WITHOUT OIDS;
    CREATE INDEX cyclic_rawdata_rterid ON cyclic_rawdata(rterid);

    GRANT INSERT ON cyclic_rawdata TO rtevparser;

-- TABLE: notes
-- This table is purely to make notes, connected to different
-- records in the database
--
    CREATE TABLE notes (
        ntid          SERIAL NOT NULL,
        reftbl        CHAR NOT NULL,    -- S=systems, R=rtevalruns
        refid         INTEGER NOT NULL, -- reference id, to the corresponding table
        notes         TEXT NOT NULL,
        createdby     VARCHAR(48),
        created       TIMESTAMP WITH TIME ZONE NOT NULL DEFAULT CURRENT_TIMESTAMP,
        PRIMARY KEY(ntid)
    ) WITH OIDS;
    CREATE INDEX notes_refid ON notes(reftbl,refid);
