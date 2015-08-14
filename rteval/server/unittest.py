import sys, threading, time, signal, libxml2
from optparse import OptionParser
from rteval_testserver import RTevald
from Logger import Logger

sys.path.insert(0,'..')
sys.path.insert(0,'../rteval')
sys.path.insert(0,'rteval')
import rtevalclient

class ServerThread(threading.Thread):
    def __init__(self, port):
        threading.Thread.__init__(self)
        self.port = port
        self.log = Logger('unit-test-server.log','rteval-xmlrpc-testsrv')

        parser = OptionParser()
        parser.add_option("-L", "--listen", action="store", dest="listen", default="127.0.0.1",
                          help="Which interface to listen to [default: %default]", metavar="IPADDR")
        parser.add_option("-P", "--port", action="store", type="int", dest="port", default=self.port,
                          help="Which port to listen to [default: %default]",  metavar="PORT")

        (options, args) = parser.parse_args()

        self.child = RTevald(options, self.log)

    def run(self):
        self.child.StartServer()

    def stop(self):
        self.child.StopServer()

    def sigcatch(self, signum, frame):
        print "Shutting down"
        self.stop()


class ClientTest(object):
    def __init__(self, port):
        self.client = rtevalclient.rtevalclient("http://localhost:%s/rteval/API1/" % port)

    def __prepare_data(self):
        d = libxml2.newDoc("1.0")
        n = libxml2.newNode('TestNode1')
        d.setRootElement(n)
        n2 = n.newTextChild(None, 'TestNode2','Just a little test')
        n2.newProp('test','true')
        for i in range(1,5):
            n2 = n.newTextChild(None, 'TestNode3', 'Test line %i' %i)
        self.testdoc = d

    def RunTest(self):
        try:
            print "** Creating XML test document"
            self.__prepare_data()
            self.testdoc.saveFormatFileEnc('-','UTF-8', 1)

            print "** Client test [1]: Hello(): %s" % str(self.client.Hello())
            status = self.client.SendReport(self.testdoc)
            print "** Client test [2]; SendReport(xmlDoc): %s" % str(status)
        except Exception, e:
            raise Exception("XML-RPC client test failed: %s" % str(e))


def unit_test(rootdir):
    ret = 1
    try:
        # Prepare server and client objects
        srvthread = ServerThread('65432')
        clienttest = ClientTest('65432')
        signal.signal(signal.SIGINT, srvthread.sigcatch)

        # Start a local XML-RPC test server
        srvthread.start()
        print "** Waiting 2 seconds for server to settle"
        time.sleep(2)

        # Start the client test
        print "** Starting client tests"
        clienttest.RunTest()
        ret = 0
    except Exception, e:
        print "** EXCEPTION: %s" % str(e)
    finally:
        # Stop the local XML-RPC test server
        srvthread.stop()
    return ret

if __name__ == "__main__":
    sys.exit(unit_test('..'))
