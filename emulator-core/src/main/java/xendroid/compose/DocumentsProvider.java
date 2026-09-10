// SPDX-License-Identifier: WTFPL
package xendroid.compose;

// R lives in the module namespace (xendroid.compose.core), not this file's package.
import xendroid.compose.core.R;

import static android.os.Build.VERSION.SDK_INT;

import android.content.res.AssetFileDescriptor;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.graphics.Point;
import android.os.Build;
import android.os.CancellationSignal;
import android.os.ParcelFileDescriptor;

import androidx.annotation.Nullable;

import java.io.FileInputStream;
import java.io.FileNotFoundException;

import android.provider.DocumentsContract.Root;

import android.provider.DocumentsContract.Document;
import android.webkit.MimeTypeMap;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Collections;
import java.util.LinkedList;

//modify from  https://github.com/android/storage-samples/blob/main/StorageProvider/Application/src/main/java/com/example/android/storageprovider/MyCloudProvider.java
public class DocumentsProvider extends android.provider.DocumentsProvider{

    //private static final String TAG = "MyCloudProvider";

    // Use these as the default columns to return information about a root if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_ROOT_PROJECTION = new String[]{
            Root.COLUMN_ROOT_ID,
            Root.COLUMN_MIME_TYPES,
            Root.COLUMN_FLAGS,
            Root.COLUMN_ICON,
            Root.COLUMN_TITLE,
            Root.COLUMN_SUMMARY,
            Root.COLUMN_DOCUMENT_ID,
            Root.COLUMN_AVAILABLE_BYTES
    };

    // Use these as the default columns to return information about a document if no specific
    // columns are requested in a query.
    private static final String[] DEFAULT_DOCUMENT_PROJECTION = new String[]{
            Document.COLUMN_DOCUMENT_ID,
            Document.COLUMN_MIME_TYPE,
            Document.COLUMN_DISPLAY_NAME,
            Document.COLUMN_LAST_MODIFIED,
            Document.COLUMN_FLAGS,
            Document.COLUMN_SIZE
    };

    // No official policy on how many to return, but make sure you do limit the number of recent
    // and search results.
    private static final int MAX_SEARCH_RESULTS = 20;
    private static final int MAX_LAST_MODIFIED = 5;

    private static final String ROOT = "root";

    private static final File baseDir(){
        return Application.get_app_data_dir();
    }

    @Override
    public boolean onCreate() {
        return true;
    }
    @Override
    public Cursor queryRoots(String[] projection) throws FileNotFoundException {
        //Log.v(TAG, "queryRoots");

        // Create a cursor with either the requested fields, or the default projection.  This
        // cursor is returned to the Android system picker UI and used to display all roots from
        // this provider.
        final MatrixCursor result = new MatrixCursor(resolveRootProjection(projection));

        // If user is not logged in, return an empty root cursor.  This removes our provider from
        // the list entirely.
        /*if (!isUserLoggedIn()) {
            return result;
        }*/

        // It's possible to have multiple roots (e.g. for multiple accounts in the same app) -
        // just add multiple cursor rows.
        // Construct one row for a root called "MyCloud".
        final MatrixCursor.RowBuilder row = result.newRow();

        row.add(Root.COLUMN_ROOT_ID, ROOT);
        row.add(Root.COLUMN_SUMMARY, null);

        // FLAG_SUPPORTS_CREATE means at least one directory under the root supports creating
        // documents.  FLAG_SUPPORTS_RECENTS means your application's most recently used
        // documents will show up in the "Recents" category.  FLAG_SUPPORTS_SEARCH allows users
        // to search all documents the application shares. FLAG_SUPPORTS_IS_CHILD allows
        // testing parent child relationships, available after SDK 21 (Lollipop).
        if (SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            row.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE |
                    //Root.FLAG_SUPPORTS_RECENTS |
                    Root.FLAG_SUPPORTS_SEARCH );
        } else {
            row.add(Root.COLUMN_FLAGS, Root.FLAG_SUPPORTS_CREATE |
                    //Root.FLAG_SUPPORTS_RECENTS |
                    Root.FLAG_SUPPORTS_SEARCH |
                    Root.FLAG_SUPPORTS_IS_CHILD);
        }

        // COLUMN_TITLE is the root title (e.g. what will be displayed to identify your provider).
        row.add(Root.COLUMN_TITLE, getContext().getString(R.string.app_name));

        // This document id must be unique within this provider and consistent across time.  The
        // system picker UI may save it and refer to it later.
        row.add(Root.COLUMN_DOCUMENT_ID, getDocIdForFile(baseDir()));

        // The child MIME types are used to filter the roots and only present to the user roots
        // that contain the desired type somewhere in their file hierarchy.
        row.add(Root.COLUMN_MIME_TYPES, "*/*");
        row.add(Root.COLUMN_AVAILABLE_BYTES, baseDir().getFreeSpace());
        row.add(Root.COLUMN_ICON, R.drawable.app_icon);

        return result;
    }
    @Override
    public Cursor querySearchDocuments(String rootId, String query, String[] projection)
            throws FileNotFoundException {
        //Log.v(TAG, "querySearchDocuments");

        // Create a cursor with the requested projection, or the default projection.
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        final File parent = getFileForDocId(rootId);

        // This example implementation searches file names for the query and doesn't rank search
        // results, so we can stop as soon as we find a sufficient number of matches.  Other
        // implementations might use other data about files, rather than the file name, to
        // produce a match; it might also require a network call to query a remote server.

        // Iterate through all files in the file structure under the root until we reach the
        // desired number of matches.
        final LinkedList<File> pending = new LinkedList<File>();

        // Start by adding the parent to the list of files to be processed
        pending.add(parent);

        // Do while we still have unexamined files, and fewer than the max search results
        while (!pending.isEmpty() && result.getCount() < MAX_SEARCH_RESULTS) {
            // Take a file from the list of unprocessed files
            final File file = pending.removeFirst();
            if (file.isDirectory()) {
                // If it's a directory, add all its children to the unprocessed list
                Collections.addAll(pending, file.listFiles());
            } else {
                // If it's a file and it matches, add it to the result cursor.
                if (file.getName().toLowerCase().contains(query)) {
                    includeFile(result, null, file);
                }
            }
        }
        return result;
    }
    @Override
    public AssetFileDescriptor openDocumentThumbnail(String documentId, Point sizeHint,
                                                     CancellationSignal signal)
            throws FileNotFoundException {
        //Log.v(TAG, "openDocumentThumbnail");

        final File file = getFileForDocId(documentId);
        final ParcelFileDescriptor pfd =
                ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
        return new AssetFileDescriptor(pfd, 0, AssetFileDescriptor.UNKNOWN_LENGTH);
    }
    @Override
    public Cursor queryDocument(String documentId, String[] projection) throws FileNotFoundException {
        //Log.v(TAG, "queryDocument");

        // Create a cursor with the requested projection, or the default projection.
        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        includeFile(result, documentId, null);
        return result;
    }

    @Override
    public Cursor queryChildDocuments(String parentDocumentId, String[] projection, String sortOrder) throws FileNotFoundException {
        /*Log.v(TAG, "queryChildDocuments, parentDocumentId: " +
                parentDocumentId +
                " sortOrder: " +
                sortOrder);*/

        final MatrixCursor result = new MatrixCursor(resolveDocumentProjection(projection));
        final File parent = getFileForDocId(parentDocumentId);
        for (File file : parent.listFiles()) {
            includeFile(result, null, file);
        }
        return result;
    }

    @Override
    public ParcelFileDescriptor openDocument(String documentId, String mode, @Nullable CancellationSignal signal) throws FileNotFoundException {

        final File file = getFileForDocId(documentId);
        final int accessMode = ParcelFileDescriptor.parseMode(mode);
        return ParcelFileDescriptor.open(file, accessMode);
    }

    @Override
    public boolean isChildDocument(String parentDocumentId, String documentId) {
        return documentId.startsWith(parentDocumentId+"/");
    }
    @Override
    public String createDocument(String documentId, String mimeType, String displayName)
            throws FileNotFoundException {

        File parent = getFileForDocId(documentId);
        File file = new File(parent.getPath(), displayName);
        try {
            boolean created;
            if (Document.MIME_TYPE_DIR.equals(mimeType)) {
                created = file.mkdir();
            } else {
                created = file.createNewFile();
            }
            if (!created) {
                throw new FileNotFoundException("Failed to create document with name " +
                        displayName +" and documentId " + documentId);
            }
        } catch (IOException e) {
            throw new FileNotFoundException("Failed to create document with name " +
                    displayName +" and documentId " + documentId);
        }
        return getDocIdForFile(file);
    }
    @Override
    public String renameDocument(String documentId, String displayName)
            throws FileNotFoundException {
        //Log.v(TAG, "renameDocument");
        if (displayName == null) {
            throw new FileNotFoundException("Failed to rename document, new name is null");
        }

        // Create the destination file in the same directory as the source file
        File sourceFile = getFileForDocId(documentId);
        File sourceParentFile = sourceFile.getParentFile();
        if (sourceParentFile == null) {
            throw new FileNotFoundException("Failed to rename document. File has no parent.");
        }
        File destFile = new File(sourceParentFile.getPath(), displayName);

        // Try to do the rename
        try {
            boolean renameSucceeded = sourceFile.renameTo(destFile);
            if (!renameSucceeded) {
                throw new FileNotFoundException("Failed to rename document. Renamed failed.");
            }
        } catch (Exception e) {
            //Log.w(TAG, "Rename exception : " + e.getLocalizedMessage() + e.getCause());
            throw new FileNotFoundException("Failed to rename document. Error: " + e.getMessage());
        }

        return getDocIdForFile(destFile);
    }
    void recursive_delete_sub_files(File dir) throws FileNotFoundException {
        File[] files = dir.listFiles();
        if(files == null)
            return;
        for(File file : files){
            if(file.isDirectory()) {
                recursive_delete_sub_files(file);
            }
            if(!file.delete()) {
                throw new FileNotFoundException("Failed to delete document with id " + file.getPath());
            }
        }
    }
    @Override
    public void deleteDocument(String documentId) throws FileNotFoundException {
        File file = getFileForDocId(documentId);
        if(file.isDirectory()){
            recursive_delete_sub_files(file);
        }

        if(!file.delete()) {
            throw new FileNotFoundException("Failed to delete document with id " + documentId);
        }
    }
    /*@Override
    public void removeDocument(String documentId, String parentDocumentId)
            throws FileNotFoundException {
        //Log.v(TAG, "removeDocument");
        File parent = getFileForDocId(parentDocumentId);
        File file = getFileForDocId(documentId);

        if (file == null) {
            throw new FileNotFoundException("Failed to delete document with id " + documentId);
        }

        deleteDocument(getDocIdForFile(file));
    }*/

    void copy_file(File src_file,File dst_file) throws IOException {
        FileInputStream in=new FileInputStream(src_file);
        FileOutputStream out=new FileOutputStream(dst_file);
        byte buf[]=new byte[16384];
        int n;
        while((n=in.read(buf))!=-1)
            out.write(buf,0,n);
        in.close();
        out.close();
    }
    void recursive_copy_sub_files(File src_dir,File dst_dir) throws IOException {
        File[] files = src_dir.listFiles();
        if(files == null)
            return;
        for(File src_file : files){

            File dst_file = new File(dst_dir.getPath(),src_file.getName());
            if(src_file.isDirectory()) {
                if(dst_file.mkdir())
                    recursive_copy_sub_files(src_file,dst_file);
                else
                    throw new IOException("Failed to create directory " + dst_file.getPath());
            }
            else{
                if(dst_file.createNewFile()) {
                    copy_file(src_file,dst_file);
                }
                else
                    throw new IOException("Failed to create file " + dst_file.getPath());
            }
        }
    }
    @Override
    public String copyDocument(String sourceDocumentId, String targetParentDocumentId)
            throws FileNotFoundException {
        File src_file = getFileForDocId(sourceDocumentId);
        File dst_file = new File(getFileForDocId(targetParentDocumentId).getPath(), src_file.getName());

        try{
            if(src_file.isDirectory()){
                dst_file.mkdir();
                recursive_copy_sub_files(src_file,dst_file);
            }
            else{
                copy_file(src_file,dst_file);
            }
        }
        catch(IOException e){
            throw new FileNotFoundException("Failed to copy document " + sourceDocumentId);
        }
        return getDocIdForFile(dst_file);
    }

    @Override
    public String moveDocument(String sourceDocumentId, String sourceParentDocumentId,
                               String targetParentDocumentId) throws FileNotFoundException {
        try {
            File src_file = getFileForDocId(sourceDocumentId);
            File dst_file = new File(getFileForDocId(targetParentDocumentId), src_file.getName());
            String newDocumentId = copyDocument(sourceDocumentId,getDocIdForFile(dst_file));
            deleteDocument(sourceDocumentId);
            return newDocumentId;
        } catch (FileNotFoundException e) {
            throw new FileNotFoundException("Failed to move document " + sourceDocumentId);
        }
    }

    @Override
    public String getDocumentType(String documentId) throws FileNotFoundException {
        File file = getFileForDocId(documentId);
        return getTypeForFile(file);
    }

    /**
     * @param projection the requested root column projection
     * @return either the requested root column projection, or the default projection if the
     * requested projection is null.
     */
    private static String[] resolveRootProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_ROOT_PROJECTION;
    }

    private static String[] resolveDocumentProjection(String[] projection) {
        return projection != null ? projection : DEFAULT_DOCUMENT_PROJECTION;
    }


    /**
     * Get a file's MIME type
     *
     * @param file the File object whose type we want
     * @return the MIME type of the file
     */
    private static String getTypeForFile(File file) {
        if (file.isDirectory()) {
            return Document.MIME_TYPE_DIR;
        } else {
            return getTypeForName(file.getName());
        }
    }

    /**
     * Get the MIME data type of a document, given its filename.
     *
     * @param name the filename of the document
     * @return the MIME data type of a document
     */
    private static String getTypeForName(String name) {
        final int lastDot = name.lastIndexOf('.');
        if (lastDot >= 0) {
            final String extension = name.substring(lastDot + 1);
            final String mime = MimeTypeMap.getSingleton().getMimeTypeFromExtension(extension);
            if (mime != null) {
                return mime;
            }
        }
        return "application/octet-stream";
    }

    /**
     * Get the document ID given a File.  The document id must be consistent across time.  Other
     * applications may save the ID and use it to reference documents later.
     * <p/>
     * This implementation is specific to this demo.  It assumes only one root and is built
     * directly from the file structure.  However, it is possible for a document to be a child of
     * multiple directories (for example "android" and "images"), in which case the file must have
     * the same consistent, unique document ID in both cases.
     *
     * @param file the File whose document ID you want
     * @return the corresponding document ID
     */
    private String getDocIdForFile(File file) {
        return file.getAbsolutePath();
    }

    /**
     * Add a representation of a file to a cursor.
     *
     * @param result the cursor to modify
     * @param docId  the document ID representing the desired file (may be null if given file)
     * @param file   the File object representing the desired file (may be null if given docID)
     * @throws FileNotFoundException
     */
    private void includeFile(MatrixCursor result, String docId, File file)
            throws FileNotFoundException {
        if (docId == null) {
            docId = getDocIdForFile(file);
        } else {
            file = getFileForDocId(docId);
        }

        int flags = 0;

        if (file.isDirectory()) {
            flags |= Document.FLAG_DIR_SUPPORTS_CREATE;

            flags |= Document.FLAG_SUPPORTS_DELETE;
            if (SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                flags |= Document.FLAG_SUPPORTS_RENAME;
            }
            if (SDK_INT >= Build.VERSION_CODES.N) {
                flags |= Document.FLAG_SUPPORTS_MOVE;
                flags |= Document.FLAG_SUPPORTS_COPY;
            }
        } else if (file.canWrite()) {
            // If the file is writable set FLAG_SUPPORTS_WRITE and
            // FLAG_SUPPORTS_DELETE
            flags |= Document.FLAG_SUPPORTS_WRITE;
            flags |= Document.FLAG_SUPPORTS_DELETE;

            // Add SDK specific flags if appropriate
            if (SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                flags |= Document.FLAG_SUPPORTS_RENAME;
            }
            if (SDK_INT >= Build.VERSION_CODES.N) {
                //flags |= Document.FLAG_SUPPORTS_REMOVE;
                flags |= Document.FLAG_SUPPORTS_MOVE;
                flags |= Document.FLAG_SUPPORTS_COPY;
            }
        }

        final String displayName = file.getName();
        final String mimeType = getTypeForFile(file);

        if (mimeType.startsWith("image/")) {
            // Allow the image to be represented by a thumbnail rather than an icon
            flags |= Document.FLAG_SUPPORTS_THUMBNAIL;
        }

        final MatrixCursor.RowBuilder row = result.newRow();
        row.add(Document.COLUMN_DOCUMENT_ID, docId);
        row.add(Document.COLUMN_DISPLAY_NAME, displayName);
        row.add(Document.COLUMN_SIZE, file.length());
        row.add(Document.COLUMN_MIME_TYPE, mimeType);
        row.add(Document.COLUMN_LAST_MODIFIED, file.lastModified());
        row.add(Document.COLUMN_FLAGS, flags);

        // Add a custom icon
        row.add(Document.COLUMN_ICON, R.drawable.app_icon);
    }

    /**
     * Translate your custom URI scheme into a File object.
     *
     * @param docId the document ID representing the desired file
     * @return a File represented by the given document ID
     * @throws java.io.FileNotFoundException
     */
    private File getFileForDocId(String docId) throws FileNotFoundException {
        File f = new File(docId);
        if (!f.exists()) throw new FileNotFoundException(f.getAbsolutePath() + " not found");
        return f;
    }
}
