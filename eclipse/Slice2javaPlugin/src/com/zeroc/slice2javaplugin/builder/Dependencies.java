// **********************************************************************
//
// Copyright (c) 2003-2008 ZeroC, Inc. All rights reserved.
//
// This plug-in is provided to you under the terms and conditions
// of the Eclipse Public License Version 1.0 ("EPL"). A copy of
// the EPL is available at http://www.eclipse.org/legal/epl-v10.html.
//
// **********************************************************************

package com.zeroc.slice2javaplugin.builder;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.StringReader;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.eclipse.core.filesystem.EFS;
import org.eclipse.core.filesystem.IFileStore;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.w3c.dom.Text;
import org.xml.sax.SAXException;

import com.zeroc.slice2javaplugin.Activator;

class Dependencies
{
    Dependencies(IProject project)
    {
        _project = project;
    }

    public void updateDependencies(String allDependencies)
        throws CoreException
    {
        try
        {
            BufferedReader in = new BufferedReader(new StringReader(allDependencies));
            StringBuffer depline = new StringBuffer();
            String line;

            while((line = in.readLine()) != null)
            {
                if(line.length() == 0)
                {
                    continue;
                }
                else if(line.endsWith("\\"))
                {
                    depline.append(line.substring(0, line.length() - 1));
                }
                else
                {
                    depline.append(line);

                    //
                    // It's easier to split up the filenames if we first convert
                    // Windows
                    // path separators into Unix path separators.
                    //
                    char[] chars = depline.toString().toCharArray();
                    int pos = 0;
                    while(pos < chars.length)
                    {
                        if(chars[pos] == '\\')
                        {
                            if(pos + 1 < chars.length)
                            {
                                //
                                // Only convert the backslash if it's not an
                                // escape.
                                //
                                if(chars[pos + 1] != ' ' && chars[pos + 1] != ':' && chars[pos + 1] != '\r'
                                        && chars[pos + 1] != '\n')
                                {
                                    chars[pos] = '/';
                                }
                            }
                        }
                        ++pos;
                    }

                    //
                    // Split the dependencies up into filenames. Note that
                    // filenames containing
                    // spaces are escaped and the initial file may have escaped
                    // colons
                    // (e.g., "C\:/Program\ Files/...").
                    //
                    java.util.ArrayList<String> l = new java.util.ArrayList<String>();
                    StringBuffer file = new StringBuffer();
                    pos = 0;
                    while(pos < chars.length)
                    {
                        if(Character.isWhitespace(chars[pos]))
                        {
                            if(file.length() > 0)
                            {
                                l.add(file.toString());
                                file = new StringBuffer();
                            }
                        }
                        else if(chars[pos] != '\\') // Skip backslash of an
                        // escaped character.
                        {
                            file.append(chars[pos]);
                        }
                        ++pos;
                    }
                    if(file.length() > 0)
                    {
                        l.add(file.toString());
                    }

                    // Remove the trailing ':'
                    String source = l.remove(0);

                    pos = source.lastIndexOf(':');
                    assert (pos == source.length() - 1);
                    source = source.substring(0, pos);

                    IFile sourceFile = _project.getFile(source);
                    Iterator<String> p = l.iterator();
                    while(p.hasNext())
                    {
                        IFile f = _project.getFile(p.next());
                        Set<IFile> dependents = reverseSliceSliceDependencies.get(f);
                        if(dependents == null)
                        {
                            dependents = new HashSet<IFile>();
                            reverseSliceSliceDependencies.put(f, dependents);
                        }
                        dependents.add(sourceFile);
                    }

                    Set<IFile> dependents = new HashSet<IFile>();
                    sliceSliceDependencies.put(sourceFile, dependents);
                    p = l.iterator();
                    while(p.hasNext())
                    {
                        dependents.add(_project.getFile(p.next()));
                    }

                    depline = new StringBuffer();
                }
            }
        }
        catch(java.io.IOException ex)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "Unable to read dependencies from slice translator: " + ex, null));
        }
    }

    public void read()
        throws CoreException
    {
        IFileStore dependencies = getDependenciesStore();
        if(!dependencies.fetchInfo(EFS.NONE, null).exists())
        {
            return;
        }
        InputStream in = dependencies.openInputStream(EFS.NONE, null);

        try
        {
            Document doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().parse(new BufferedInputStream(in));
            DependenciesParser parser = new DependenciesParser(_project);
            parser.visit(doc);
            sliceSliceDependencies = parser.sliceSliceDependencies;
            reverseSliceSliceDependencies = parser.reverseSliceSliceDependencies;
            sliceJavaDependencies = parser.sliceJavaDependencies;
        }
        catch(SAXException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error reading dependencies", e));
        }
        catch(ParserConfigurationException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error reading dependencies", e));
        }
        catch(IOException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error reading dependencies", e));
        }
    }

    public void write()
        throws CoreException
    {
        // Create a DOM of the map.
        Document doc = null;
        try
        {
            doc = DocumentBuilderFactory.newInstance().newDocumentBuilder().newDocument();
        }
        catch(ParserConfigurationException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error writing dependencies", e));
        }

        Element root = doc.createElement("dependencies");
        doc.appendChild(root);

        writeDependencies(sliceSliceDependencies, doc, "sliceSliceDependencies", root);
        writeDependencies(reverseSliceSliceDependencies, doc, "reverseSliceSliceDependencies", root);
        writeDependencies(sliceJavaDependencies, doc, "sliceJavaDependencies", root);

        // Write the DOM to the dependencies.xml file.
        TransformerFactory transfac = TransformerFactory.newInstance();
        Transformer trans = null;
        try
        {
            trans = transfac.newTransformer();
        }
        catch(TransformerConfigurationException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error writing dependencies", e));
        }
        // tf.setAttribute("indent-number", 4);

        // trans.setOutputProperty(OutputKeys.OMIT_XML_DECLARATION, "yes");
        trans.setOutputProperty(OutputKeys.INDENT, "yes");
        trans.setOutputProperty(OutputKeys.ENCODING, "UTF8");
        trans.setOutputProperty(OutputKeys.INDENT, "yes");
        trans.setOutputProperty(OutputKeys.METHOD, "XML");

        IFileStore dependencies = getDependenciesStore();
        OutputStream out = dependencies.openOutputStream(EFS.NONE, null);
        StreamResult result = new StreamResult(new BufferedOutputStream(out));
        DOMSource source = new DOMSource(doc);
        try
        {
            trans.transform(source, result);
        }
        catch(TransformerException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error writing dependencies", e));
        }
        try
        {
            out.close();
        }
        catch(IOException e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                    "internal error writing dependencies", e));
        }
    }

    private void writeDependencies(Map<IFile, Set<IFile>> map, Document doc, String name, Element root)
    {
        Element jsd = doc.createElement(name);
        root.appendChild(jsd);

        Iterator<Map.Entry<IFile, Set<IFile>>> p = map.entrySet().iterator();
        while(p.hasNext())
        {
            Map.Entry<IFile, Set<IFile>> e = p.next();
            Element entry = doc.createElement("entry");
            jsd.appendChild(entry);

            Element key = doc.createElement("key");
            entry.appendChild(key);
            Text text = doc.createTextNode(e.getKey().getProjectRelativePath().toString());
            key.appendChild(text);

            Element value = doc.createElement("value");
            entry.appendChild(value);

            Iterator<IFile> q = e.getValue().iterator();
            while(q.hasNext())
            {
                IFile f = q.next();
                Element elem = doc.createElement("file");
                value.appendChild(elem);
                text = doc.createTextNode(f.getProjectRelativePath().toString());
                elem.appendChild(text);
            }
        }
    }

    private IFileStore getDependenciesStore()
        throws CoreException
    {
        IPath name = new Path(_project.getName());
        IFileStore store = EFS.getLocalFileSystem().getStore(Activator.getDefault().getStateLocation()).getFileStore(
                name);
        if(!store.fetchInfo(EFS.NONE, null).exists())
        {
            store.mkdir(EFS.NONE, null);
        }
        return store.getFileStore(new Path("dependencies.xml"));
    }

    static class DependenciesParser
    {
        private IProject _project;
        
        Map<IFile, Set<IFile>> sliceSliceDependencies = new java.util.HashMap<IFile, Set<IFile>>();
        Map<IFile, Set<IFile>> reverseSliceSliceDependencies = new java.util.HashMap<IFile, Set<IFile>>();
        Map<IFile, Set<IFile>> sliceJavaDependencies = new java.util.HashMap<IFile, Set<IFile>>();

        private Node findNode(Node n, String qName)
            throws SAXException
        {
            NodeList children = n.getChildNodes();
            for(int i = 0; i < children.getLength(); ++i)
            {
                Node child = children.item(i);
                if(child.getNodeType() == Node.ELEMENT_NODE && child.getNodeName().equals(qName))
                {
                    return child;
                }
            }
            throw new SAXException("no such node: " + qName);
        }

        private String getText(Node n)
            throws SAXException
        {
            NodeList children = n.getChildNodes();
            if(children.getLength() == 1 && children.item(0).getNodeType() == Node.TEXT_NODE)
            {
                return children.item(0).getNodeValue();
            }
            throw new SAXException("no text element");
        }

        private List<String> processFiles(Node n)
            throws SAXException
        {
            List<String> files = new ArrayList<String>();
            NodeList children = n.getChildNodes();
            for(int i = 0; i < children.getLength(); ++i)
            {
                Node child = children.item(i);
                if(child.getNodeType() == Node.ELEMENT_NODE && child.getNodeName().equals("file"))
                {
                    files.add(getText(child));
                }
            }
            return files;
        }

        public void visitDependencies(Map<IFile, Set<IFile>> map, Node n)
            throws SAXException
        {
            NodeList children = n.getChildNodes();
            for(int i = 0; i < children.getLength(); ++i)
            {
                Node child = children.item(i);
                if(child.getNodeType() == Node.ELEMENT_NODE && child.getNodeName().equals("entry"))
                {
                    IFile key = _project.getFile(new Path(getText(findNode(child, "key"))));

                    Node value = findNode(child, "value");
                    List<String> files = processFiles(value);
                    Set<IFile> f = new HashSet<IFile>();
                    Iterator<String> p = files.iterator();
                    while(p.hasNext())
                    {
                        f.add(_project.getFile(new Path(p.next())));
                    }

                    map.put(key, f);
                }
            }
        }

        public void visit(Node doc)
            throws SAXException
        {
            Node dependencies = findNode(doc, "dependencies");
            visitDependencies(sliceSliceDependencies, findNode(dependencies, "sliceSliceDependencies"));
            visitDependencies(reverseSliceSliceDependencies, findNode(dependencies, "reverseSliceSliceDependencies"));
            visitDependencies(sliceJavaDependencies, findNode(dependencies, "sliceJavaDependencies"));
        }
        
        DependenciesParser(IProject project)
        {
            _project = project;
        }
    }

    // A map of slice to dependencies.
    //
    // sliceSliceDependencies is the set of slice files that depend on the IFile
    // (the output of slice2java --depend).
    //
    // _reverseSliceSliceDependencies is the reverse.
    Map<IFile, Set<IFile>> sliceSliceDependencies = new java.util.HashMap<IFile, Set<IFile>>();
    Map<IFile, Set<IFile>> reverseSliceSliceDependencies = new java.util.HashMap<IFile, Set<IFile>>();

    // A map of slice file to java source files.
    Map<IFile, Set<IFile>> sliceJavaDependencies = new java.util.HashMap<IFile, Set<IFile>>();
    
    private IProject _project;
}
