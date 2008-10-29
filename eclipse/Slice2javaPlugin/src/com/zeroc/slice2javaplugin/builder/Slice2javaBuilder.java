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

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IFolder;
import org.eclipse.core.resources.IMarker;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.IResourceVisitor;
import org.eclipse.core.resources.IncrementalProjectBuilder;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.SWT;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.console.ConsolePlugin;
import org.eclipse.ui.console.IConsole;
import org.eclipse.ui.console.IConsoleManager;
import org.eclipse.ui.console.MessageConsole;
import org.eclipse.ui.console.MessageConsoleStream;

import com.zeroc.slice2javaplugin.Activator;
import com.zeroc.slice2javaplugin.properties.Configuration;


public class Slice2javaBuilder extends IncrementalProjectBuilder
{
    public static final String BUILDER_ID = "com.zeroc.Slice2javaPlugin.Slice2javaBuilder";
    
    /*
     * (non-Javadoc)
     * 
     * @see org.eclipse.core.internal.events.InternalBuilder#build(int,
     * java.util.Map, org.eclipse.core.runtime.IProgressMonitor)
     */
    protected IProject[] build(int kind, Map args, IProgressMonitor monitor)
        throws CoreException
    {
        BuildState state = new BuildState(getProject());
        state.dependencies.read();

        if(kind == FULL_BUILD)
        {
            fullBuild(state, monitor);
        }
        else
        {
            IResourceDelta delta = getDelta(getProject());
            if(delta == null)
            {
                fullBuild(state, monitor);
            }
            else
            {
                incrementalBuild(state, delta, monitor);
            }
        }

        return null;
    }

    protected void clean(IProgressMonitor monitor)
        throws CoreException
    {
        BuildState state = new BuildState(getProject());
        
        // Don't read the existing dependencies. That will have the
        // effect of trashing them.
        
        // Now, clean the generated sub-directory.
        Set<IFile> files = new HashSet<IFile>();
        getResources(files, state.generated.members());
        
        for(Iterator<IFile> p = files.iterator(); p.hasNext();)
        {
            p.next().delete(true, false, monitor);
        }

        state.dependencies.write();
    }
    
    // XXX: Is this necessary, or can I just read directly from
    // the build method?
    static class StreamReaderThread extends Thread
    {
        public StreamReaderThread(InputStream in, StringBuffer out)
        {
            _in = new BufferedReader(new InputStreamReader(in), 1024);
            _out = out;
        }

        public void run()
        {
            try
            {
                char[] buf = new char[1024];
                while(true)
                {
                    int read = _in.read(buf);
                    if(read == -1)
                    {
                        break;
                    }
                    _out.append(buf, 0, read);
                }
            }
            catch(Exception e)
            {
            }
            finally
            {
                try
                {
                    _in.close();
                }
                catch(IOException e1)
                {
                    e1.printStackTrace();
                }
            }
        }

        private StringBuffer _out;
        private BufferedReader _in;
    }
    
    static class BuildState
    {
        BuildState(IProject project) throws CoreException
        {
            config = new Configuration(project);

            generated = getGenerated(project);
            _sourceLocations = new HashSet<IFolder>();
            for(Iterator<String> p = config.getSliceSourceDirs().iterator(); p.hasNext();)
            {
                _sourceLocations.add(project.getFolder(p.next()));
            }
            dependencies = new Dependencies(project);
            if(config.getConsole())
            {
                initializeConsole();
                out = _consoleout;
                err = _consoleerr;
            }
        }

        private IFolder getGenerated(IProject project)
            throws CoreException
        {
            IFolder generated = project.getFolder(config.getGeneratedDir());
            if(!generated.exists())
            {
                throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID,
                        "Plugin configuration incorrect: missing generated dir", null));
            }
            return generated;
        }
        
        public boolean filter(IFile file)
        {
            if(file.getFileExtension().equals("ice"))
            {
                IFolder folder = (IFolder)file.getParent();
                if(_sourceLocations.contains(folder))
                {
                    return true;
                }
            }
            return false;
        }
        
        synchronized static private void initializeConsole()
        {
            if(_consoleout == null)
            {
                MessageConsole console = new MessageConsole("slice2java", null);
                IConsole[] ics = new IConsole[1];
                ics[0] = console;
                IConsoleManager csmg = ConsolePlugin.getDefault().getConsoleManager();
                csmg.addConsoles(ics);
                csmg.showConsoleView(console);
    
                _consoleout = console.newMessageStream();
                _consoleerr = console.newMessageStream();
    
                final Display display = PlatformUI.getWorkbench().getDisplay();
                display.syncExec(new Runnable() {
                        public void run() {
                                _consoleerr.setColor(display.getSystemColor(SWT.COLOR_RED));
                        }
                });
            }
        }

        Configuration config;
        Dependencies dependencies;
        IFolder generated;
        private Set<IFolder> _sourceLocations;
        private MessageConsoleStream out = null;
        private MessageConsoleStream err = null;
 
        static private MessageConsoleStream _consoleout = null;
        static private MessageConsoleStream _consoleerr = null;
    };
    
    class SampleResourceVisitor implements IResourceVisitor
    {
        SampleResourceVisitor(BuildState state, Set<IFile> candidates)
        {
            _state = state;
            _candidates = candidates;
        }

        public boolean visit(IResource resource)
            throws CoreException
        {
            if(resource instanceof IFile)
            {
                IFile file = (IFile)resource;
                if(_state.filter(file))
                {
                    _candidates.add((IFile) resource);
                }
            }
            return true;
        }

        private BuildState _state;
        private Set<IFile> _candidates;
    };

    class SampleDeltaVisitor implements IResourceDeltaVisitor
    {
        SampleDeltaVisitor(BuildState state, Set<IFile> candidates, List<IFile> removed)
        {
            _state = state;
            _candidates = candidates;
            _removed = removed;
        }

        public boolean visit(IResourceDelta delta)
            throws CoreException
        {
            IResource resource = delta.getResource();
            if(resource instanceof IFile)
            {
                IFile file = (IFile)resource;
                if(_state.filter(file))
                {
                    switch (delta.getKind())
                    {
                    case IResourceDelta.ADDED:
                    case IResourceDelta.CHANGED:
                        _candidates.add(file);
                        break;
                    case IResourceDelta.REMOVED:
                        _removed.add(file);
                        break;
                    }
                }
            }
            return true;
        }

        private BuildState _state;
        private Set<IFile> _candidates;
        private List<IFile> _removed;
    }

    private int build(BuildState state, Set<IFile> files, boolean depend, StringBuffer out)
        throws CoreException
    {
        // Clear the output buffer.
        out.setLength(0);

        List<String> cmd = new LinkedList<String>();
        String translator = state.config.getTranslator();
        if(translator == null)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, "Cannot locate slice2java translator: please fix ice install location", null));
        }
        
        cmd.add(translator);
        if(depend)
        {
            cmd.add("--depend");
        }
        else
        {
            cmd.add("--output-dir=" + state.generated.getProjectRelativePath().toString());
        }
        
        cmd.addAll(state.config.getCommandLine());
        
        for(Iterator<IFile> p = files.iterator(); p.hasNext();)
        {
            cmd.add(p.next().getProjectRelativePath().toString());
        }

        if(state.out != null)
        {
            for(Iterator<String> p = cmd.iterator(); p.hasNext();)
            {
                state.out.print(p.next());
                state.out.print(" ");
            }
            state.out.println("");
        }
        ProcessBuilder builder = new ProcessBuilder(cmd);
        builder.redirectErrorStream(true);

        IPath rootLocation = getProject().getLocation();
        builder.directory(rootLocation.toFile());
        Map<String, String> env = builder.environment();
        state.config.setupSharedLibraryPath(env);

        try
        {
            Process proc = builder.start();

            StreamReaderThread outThread = new StreamReaderThread(proc.getInputStream(), out);

            outThread.start();

            int status = proc.waitFor();

            outThread.join();

            if(status != 0 && state.err != null)
            {
                state.err.println("slice2java status: " + status);
            }

            return status;
        }
        catch(Exception e)
        {
            throw new CoreException(new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.toString(), null));
        }
        // not reached
    }

    private void
    createMarker(BuildState state, IFile source, IPath filename, int line, String error)
        throws CoreException
    {
        // Process the error.
        IPath dir = getProject().getLocation();
        
        IFile file = null;
        if(filename != null && dir.isPrefixOf(filename))
        {
            // Locate the file within the project.
            file = getProject().getFile(filename.removeFirstSegments(dir.segmentCount()));

            // Only place markers in the current source file, or those source
            // files that we're not building.
            if(!file.equals(source) && state.filter(file))
            {
                return;
            }
        }
        
        IMarker marker = source.createMarker(IMarker.PROBLEM);
        marker.setAttribute(IMarker.MESSAGE, error);
        marker.setAttribute(IMarker.SEVERITY, IMarker.SEVERITY_ERROR);
        if(file != null && file.equals(source))
        {
            marker.setAttribute(IMarker.LINE_NUMBER, line);
        }
    }
    
    private void createMarkers(BuildState state, IFile source, String output)
        throws CoreException
    {
        output = output.trim();
        if(output.length() == 0)
        {
            return;
        }
        
        String[] lines = output.split("\n");
        
        IPath filename = null;
        int line = -1;
        StringBuffer error = new StringBuffer();
        
        boolean continuation = false;
        
        for(int i = 0; i < lines.length; ++i)
        {
            if(continuation)
            {
                if(lines[i].startsWith(" "))
                {
                    // continuation of the previous error
                    error.append(lines[i]);
                    //error.append("\n");
                    continue;
                }
                else
                {
                    // Process the error.
                    createMarker(state, source, filename, line, error.toString());
                }
            }
            
            // We're on a new error.
            error.setLength(0);
            continuation = false;
            
            // Ignore.
            if(lines[i].contains("errors in preprocessor") || lines[i].contains("error in preprocessor"))
            {
                continue;
            }
            
            try
            {
                int start = 0;
                int end;
                // Handle drive letters.
                if(lines[i].length() > 2 && lines[i].charAt(1) == ':')
                {
                    end = lines[i].indexOf(':', 2);
                }
                else
                {
                    end = lines[i].indexOf(':');
                }
                if(end != -1)
                {
                    filename = new Path(lines[i].substring(start, end));
                    start = end+1;
                    end = lines[i].indexOf(':', start);
                    if(end != -1)
                    {
                        line = Integer.parseInt(lines[i].substring(start, end));
                        start = end+1;
                        
                        error.append(lines[i].substring(start, lines[i].length()));
    
                        continuation = true;
                        continue;
                    }
                }
            }
            catch(NumberFormatException e)
            {
                // Ignore the error is of some unknown format.
            }
            // Unknown format.
            createMarker(state, source, null, -1, lines[i]);
        }

        if(continuation)
        {
            createMarker(state, source, filename, line, error.toString());
        }
    }
    
    private void getTimestamps(Map<IFile, Long> resources, IResource[] members)
        throws CoreException
    {
        for(int i = 0; i < members.length; ++i)
        {
            if(members[i] instanceof IFile)
            {
                resources.put((IFile) members[i], new Long(members[i].getLocalTimeStamp()));
            }
            else if(members[i] instanceof IFolder)
            {
                getTimestamps(resources, ((IFolder) members[i]).members());
            }
        }
    }

    private void getResources(Set<IFile> files, IResource[] members)
        throws CoreException
    {
        for(int i = 0; i < members.length; ++i)
        {
            if(members[i] instanceof IFile)
            {
                files.add((IFile) members[i]);
            }
            else if(members[i] instanceof IFolder)
            {
                getResources(files, ((IFolder) members[i]).members());
            }
        }
    }

    private void fullBuild(BuildState state, final IProgressMonitor monitor)
        throws CoreException
    {
        clean(monitor);
        Set<IFile> candidates = new HashSet<IFile>();
        getProject().accept(new SampleResourceVisitor(state, candidates));
        
        if(candidates.isEmpty())
        {
            return;
        }

        if(state.out != null)
        {
            state.out.println("--- full build ---");
    
            state.out.println("full candidate list");
            // This is a complete list of slice files.
            for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
            {
                state.out.println("    " + p.next().getProjectRelativePath().toString());
            }
            state.out.println("phase: regenerating java source files");
        }
        
        StringBuffer out = new StringBuffer();

        Set<IFile> depends = new HashSet<IFile>();
        
        Map<IFile, Long> timestamps = new HashMap<IFile, Long>();
        // Run through the build list, and build each slice file.
        for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
        {
            IFile file = p.next();
            Set<IFile> files = new HashSet<IFile>();
            files.add(file);
            
            file.deleteMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);
            
            int status = build(state, files, false, out);
            
            createMarkers(state, file, out.toString());
            if(state.out != null)
            {
                state.out.print(out.toString());
            }

            if(status != 0)
            {
                continue;
            }
            depends.add(file);

            // We need to now look at the files in the generated sub-directory,
            // and determine which files have changed. Any that have not changed
            // that used to be owned by this slice file must be added to an
            // orphan candidate list.
            Set<IFile> newGeneratedJavaFiles = new HashSet<IFile>();

            state.generated.refreshLocal(IResource.DEPTH_INFINITE, monitor);

            Map<IFile, Long> updated = new HashMap<IFile, Long>();
            getTimestamps(updated, state.generated.members());

            for(Iterator<Map.Entry<IFile, Long>> q = updated.entrySet().iterator(); q.hasNext();)
            {
                Map.Entry<IFile, Long> entry = q.next();
                Long l = timestamps.get(entry.getKey());
                // If the file is new, or the entry is newer than the one in the
                // original list, then the file has been updated.
                //
                // Mark the java file as a derived resource.
                if(l == null || entry.getValue().compareTo(l) > 0)
                {
                    newGeneratedJavaFiles.add(entry.getKey());
                    entry.getKey().setDerived(true);
                }
            }

            if(state.out != null)
            {
                state.out.println("emitted");
                for(Iterator<IFile> q = newGeneratedJavaFiles.iterator(); q.hasNext();)
                {
                    state.out.println("    " + q.next().getProjectRelativePath().toString());
                }
            }

            // Update the set of slice -> java dependencies.
            state.dependencies.sliceJavaDependencies.put(file, newGeneratedJavaFiles);

            // The current timestamps are now the updated timestamps.
            timestamps = updated;
        }

        // Update the slice->slice dependencies.
        if(state.out != null)
        {
            state.out.println("phase: updating dependencies");
        }

        // Only update the dependencies for those files with no build problems.
        if(!depends.isEmpty())
        {
            if(build(state, depends, true, out) == 0)
            {
                // Parse the new dependency set.
                state.dependencies.updateDependencies(out.toString());
                state.dependencies.write();
            }
            else if(state.err != null)
            {
                state.err.println("dependencies not updated.");
                state.err.println(out.toString());    
            }
        }
    }

    private void incrementalBuild(BuildState state, IResourceDelta delta, IProgressMonitor monitor)
        throws CoreException
    {
        Set<IFile> candidates = new HashSet<IFile>();
        List<IFile> removed = new ArrayList<IFile>();
        
        // Compute the set of candidates.
        delta.accept(new SampleDeltaVisitor(state, candidates, removed));

        if(state.out != null)
        {
            state.out.println("--- incremental build ---");
    
            state.out.println("candidate list");
            // This is a complete list of slice files.
            for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
            {
                state.out.println("   + " + p.next().getProjectRelativePath().toString());
            }
            for(Iterator<IFile> p = removed.iterator(); p.hasNext();)
            {
                state.out.println("   - " + p.next().getProjectRelativePath().toString());
            }
        }
        // Go through the removed list, removing the dependencies.
        for(Iterator<IFile> p = removed.iterator(); p.hasNext();)
        {
            IFile f = p.next();
            
            Set<IFile> dependents = state.dependencies.sliceSliceDependencies.get(f);
            Iterator<IFile> dependentsIterator = dependents.iterator();
            while(dependentsIterator.hasNext())
            {
                IFile dependent = dependentsIterator.next();
                Set<IFile> files = state.dependencies.reverseSliceSliceDependencies.get(dependent);
                if(files != null)
                {
                    files.remove(f);
                }
            }

            state.dependencies.sliceSliceDependencies.remove(f);
            state.dependencies.sliceJavaDependencies.remove(f);
        }

        // Add the removed files to the candidates set
        // prior to determining additional candidates.
        candidates.addAll(removed);

        for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
        {
            IFile f = p.next();
            
            Set<IFile> files = state.dependencies.reverseSliceSliceDependencies.get(f);
            if(files != null)
            {
                for(Iterator<IFile> q = files.iterator(); q.hasNext();)
                {
                    IFile potentialCandidate = q.next();
                    if(potentialCandidate.exists())
                    {
                        candidates.add(potentialCandidate);
                    }
                }
            }

            // If this is a file in the contained list, then remove the
            // dependency entry.
            if(removed.contains(f))
            {
                state.dependencies.reverseSliceSliceDependencies.remove(f);
            }
        }

        // Remove all the removed files from the candidates list.
        candidates.removeAll(removed);

        if(state.out != null)
        {
            state.out.println("expanded candidate list");
            // This is a complete list of slice files.
            for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
            {
                state.out.println("    " + p.next().getProjectRelativePath().toString());
            }
        }
        // If there are no candidates, we're done.
        if(candidates.isEmpty())
        {
            state.dependencies.write();
            return;
        }

        if(state.out != null)
        {
            state.out.println("phase: regenerating java source files");
        }

        // All source files contained in the generated sub-directory.
        Map<IFile, Long> timestamps = new HashMap<IFile, Long>();
        getTimestamps(timestamps, state.generated.members());

        StringBuffer out = new StringBuffer();
        
        // The orphan candidate set.
        Set<IFile> orphanCandidateSet = new HashSet<IFile>();
        // The complete set of generated java files by this build.
        Set<IFile> generatedJavaFiles = new HashSet<IFile>();
        // The set of files that we'll generate dependencies for.
        Set<IFile> depends = new HashSet<IFile>();

        // Run through the build list, and build each slice file.
        for(Iterator<IFile> p = candidates.iterator(); p.hasNext();)
        {
            IFile file = p.next();
            
            Set<IFile> files = new HashSet<IFile>();
            files.add(file);
            
            file.deleteMarkers(IMarker.PROBLEM, true, IResource.DEPTH_INFINITE);
            
            int status = build(state, files, false, out);
            
            createMarkers(state, file, out.toString());
            if(state.out != null)
            {
                state.out.print(out.toString());
            }
            
            if(status != 0)
            {
                continue;
            }
            depends.add(file);
            
            // We need to now look at the files in the generated sub-directory,
            // and determine which files have changed. Any that have not changed
            // that used to be owned by this slice file must be added to an
            // orphan candidate list.
            Set<IFile> newGeneratedJavaFiles = new HashSet<IFile>();
            
            state.generated.refreshLocal(IResource.DEPTH_INFINITE, monitor);

            Map<IFile, Long> updated = new HashMap<IFile, Long>();
            getTimestamps(updated, state.generated.members());

            for(Iterator<Map.Entry<IFile, Long>> q = updated.entrySet().iterator(); q.hasNext();)
            {
                Map.Entry<IFile, Long> entry = q.next();
                
                Long l = timestamps.get(entry.getKey());
                // If the file is new, or the entry is newer than the one in the
                // original list, then the file has been updated.
                //
                // Mark the emitted java source as a derived resource.
                if(l == null || entry.getValue().compareTo(l) > 0)
                {
                    newGeneratedJavaFiles.add(entry.getKey());                   
                    generatedJavaFiles.add(entry.getKey());
                    
                    entry.getKey().setDerived(true);
                }
            }

            if(state.out != null)
            {
                state.out.println("emitted");
                for(Iterator<IFile> q = newGeneratedJavaFiles.iterator(); q.hasNext();)
                {
                    state.out.println("    " + q.next().getProjectRelativePath().toString());
                }
            }
            // Compute the set difference between the old set and new set
            // of generated files. The difference should be added to the
            // orphan candidate set.
            Set<IFile> oldJavaFiles = state.dependencies.sliceJavaDependencies.get(file);
            if(oldJavaFiles != null)
            {
                // Compute the set difference.
                oldJavaFiles.removeAll(newGeneratedJavaFiles);
                if(state.out != null)
                {
                    state.out.println("orphans from this build");
                    for(Iterator<IFile> q = oldJavaFiles.iterator(); q.hasNext();)
                    {
                        state.out.println("    " + q.next().getProjectRelativePath().toString());
                    }
                }
                orphanCandidateSet.addAll(oldJavaFiles);
            }
            // Update the set of slice -> java dependencies.
            state.dependencies.sliceJavaDependencies.put(file, newGeneratedJavaFiles);

            // The current timestamps are now the updated timestamps.
            timestamps = updated;
        }

        // Do a set difference between the orphan candidate set
        // and the complete set of generated java source files.
        // Any remaining are complete orphans and should
        // be removed.
        orphanCandidateSet.removeAll(generatedJavaFiles);
        
        if(state.out != null)
        {
            state.out.println("orphans");
            for(Iterator<IFile> p = orphanCandidateSet.iterator(); p.hasNext();)
            {
                state.out.println("    " + p.next().getProjectRelativePath().toString());
            }
            state.out.println("phase: updating dependencies");
        }
        //
        // Remove orphans.
        //
        for(Iterator<IFile> p = orphanCandidateSet.iterator(); p.hasNext();)
        {
            p.next().delete(true, false, monitor);
        }

        // The dependencies of any files without build errors should be updated.

        // We've already added markers for any errors... Only update the
        // dependencies if no problems resulted in the build.
        if(!depends.isEmpty())
        {
            if(build(state, depends, true, out) == 0)
            {
                // Parse the new dependency set.
                state.dependencies.updateDependencies(out.toString());
                state.dependencies.write();
            }
            else if(state.err != null)
            {
                state.err.println("dependencies not updated.");
                state.err.println(out.toString());    
            }
        }
    }
}
